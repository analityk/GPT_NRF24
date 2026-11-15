#include "nrf24.h"
#ifndef GPT_HOST_TEST
#include "nrf24.h"
#include "timer1_clock.h"
#include "avr_delay.h"
#endif
#include "nrf24_ack_phy.h"

NrfAckPayloadPhy::TxResult::TxResult()
	: code(TxCode::IO_ERROR),
	  retries(0) {
}

NrfAckPayloadPhy::Timing::Timing()
	: loadTicks(0),
	  guardTicks(0),
	  ceTicks(0),
	  waitTicks(0),
	  readTicks(0),
	  pollCount(0),
	  pollSpiTicks(0),
	  observeTicks(0),
	  fifoTicks(0),
	  payloadTicks(0),
	  irqTicks(0) {
}

NrfAckPayloadPhy::NrfAckPayloadPhy(RadioHandle radio, DelayHandle delay, ClockHandle clock)
	: radio_(radio),
	  delay_(delay),
	  clock_(clock),
	  lastCompleted_(0),
	  phaseStarted_(0),
	  haveLast_(false),
	  rxFifoFullSamples_(0),
	  phase_(TxPhase::IDLE) {
}

uint16_t NrfAckPayloadPhy::rxFifoFullSamples() const {
	return rxFifoFullSamples_;
}

NrfAckPayloadPhy::TxPhase NrfAckPayloadPhy::txPhase() const {
	return phase_;
}

bool NrfAckPayloadPhy::txBusy() const {
	return phase_ != TxPhase::IDLE;
}

bool NrfAckPayloadPhy::txOnAir() const {
	return phase_ == TxPhase::ON_AIR;
}

const NrfAckPayloadPhy::Timing& NrfAckPayloadPhy::lastTiming() const {
	return timing_;
}

uint32_t NrfAckPayloadPhy::lastCompletedAt() const {
	return lastCompleted_;
}

bool NrfAckPayloadPhy::beginPtx() {
	radio_.ceLow();
	if (!radio_.writeRegister(Nrf24::CONFIG, NRF24_PHY_CONFIG_PTX)) {
		return false;
	}
	delay_.ms(NRF24_PHY_POWERUP_MS);
	return radio_.clearIrq(NRF24_PHY_IRQ_CLEAR_MASK);
}

bool NrfAckPayloadPhy::beginPrx() {
	radio_.ceLow();
	if (!radio_.writeRegister(Nrf24::CONFIG, NRF24_PHY_CONFIG_PRX)) {
		return false;
	}
	delay_.ms(NRF24_PHY_POWERUP_MS);
	if (!radio_.clearIrq(NRF24_PHY_IRQ_CLEAR_MASK)) {
		return false;
	}
	radio_.ceHigh();
	delay_.us(NRF24_PHY_PRX_SETTLE_US);
	return true;
}

bool NrfAckPayloadPhy::startTransfer(
	const uint8_t (&out)[NRF24_PHY_MAX_FRAME_BYTES], uint8_t txWidth) {
	if (txBusy() || txWidth < NRF24_PHY_MIN_FRAME_BYTES || txWidth > NRF24_PHY_MAX_FRAME_BYTES) {
		return false;
	}
	const uint32_t started = PP_PHY_MEASURE_TIMING ? clock_.now() : 0;
	if (!radio_.writeDynamicPayload(out, txWidth)) {
		return false;
	}
	if (PP_PHY_MEASURE_TIMING) {
		const uint32_t loaded = clock_.now();
		timing_ = Timing();
		timing_.loadTicks = measuredSpan(loaded - started);
		phaseStarted_ = loaded;
	}
	phase_ = TxPhase::GUARD;
	return true;
}

NrfAckPayloadPhy::TxStep NrfAckPayloadPhy::pollTransfer(
	uint8_t (&reply)[NRF24_PHY_MAX_FRAME_BYTES], uint8_t& replyWidth, TxResult& result) {
	if (phase_ == TxPhase::IDLE) {
		return TxStep::IDLE;
	}
	if (phase_ == TxPhase::GUARD) {
		replyWidth = 0;
		result = TxResult();
		if (haveLast_ &&
			static_cast<uint32_t>(clock_.now() - lastCompleted_) < NRF24_PHY_PTX_GUARD_TICKS) {
			return TxStep::PENDING;
		}
		if (PP_PHY_MEASURE_TIMING) {
			timing_.guardTicks = measuredSpan(clock_.now() - phaseStarted_);
		}
		radio_.ceHigh();
		phaseStarted_ = clock_.now();
		phase_ = TxPhase::CE_PULSE;
		return TxStep::PENDING;
	}
	if (phase_ == TxPhase::CE_PULSE) {
		if (static_cast<uint32_t>(clock_.now() - phaseStarted_) < NRF24_PHY_CE_PULSE_TICKS) {
			return TxStep::PENDING;
		}
		if (PP_PHY_MEASURE_TIMING) {
			timing_.ceTicks = measuredSpan(clock_.now() - phaseStarted_);
		}
		radio_.ceLow();
		phaseStarted_ = clock_.now();
		phase_ = TxPhase::ON_AIR;
		return TxStep::PENDING;
	}
	replyWidth = 0;
	uint8_t flags = 0;
	const uint32_t statusStart = PP_PHY_MEASURE_TIMING ? clock_.now() : 0;
	if (!radio_.status(flags)) {
		return finishError(result);
	}
	if (PP_PHY_MEASURE_TIMING) {
		++timing_.pollCount;
		const uint32_t elapsed = clock_.now() - statusStart;
		const uint32_t cumulative = static_cast<uint32_t>(timing_.pollSpiTicks) + elapsed;
		timing_.pollSpiTicks = measuredSpan(cumulative);
	}
	if ((flags & (NRF24_PHY_IRQ_TX_DONE_MASK | NRF24_PHY_IRQ_MAX_RT_MASK)) == 0) {
		const uint32_t now = clock_.now();
		if (static_cast<uint32_t>(now - phaseStarted_) < NRF24_PHY_TX_TIMEOUT_TICKS) {
			return TxStep::PENDING;
		}
		if (PP_PHY_MEASURE_TIMING) {
			markTerminal(now);
		}
		result.code = TxCode::TIMEOUT;
		if (!radio_.flushTx() || !radio_.clearIrq(NRF24_PHY_IRQ_CLEAR_MASK)) {
			result.code = TxCode::IO_ERROR;
		}
		return finish();
	}
	if (PP_PHY_MEASURE_TIMING) {
		markTerminal(clock_.now());
	}
	uint32_t stageStarted = phaseStarted_;
	uint8_t observe = 0;
	if (!radio_.readRegister(Nrf24::OBSERVE_TX, observe)) {
		return finishError(result);
	}
	if (PP_PHY_MEASURE_TIMING) {
		markReadStage(stageStarted, timing_.observeTicks);
	}
	result.retries = observe & NRF24_PHY_OBSERVE_RETRY_MASK;
	if (flags & NRF24_PHY_IRQ_MAX_RT_MASK) {
		if (!radio_.flushTx() || !radio_.clearIrq(NRF24_PHY_IRQ_CLEAR_MASK)) {
			return finishError(result);
		}
		result.code = TxCode::MAX_RT;
		return finish();
	}
	uint8_t fifo = 0;
	if (!radio_.readRegister(Nrf24::FIFO_STATUS, fifo)) {
		return finishError(result);
	}
	if (PP_PHY_MEASURE_TIMING) {
		markReadStage(stageStarted, timing_.fifoTicks);
	}
	if (fifo & NRF24_PHY_RX_EMPTY_MASK) {
		result.code = TxCode::ACK_EMPTY;
	} else {
		const bool good = radio_.readDynamicPayload(reply, replyWidth);
		if (PP_PHY_MEASURE_TIMING) {
			markReadStage(stageStarted, timing_.payloadTicks);
		}
		if (!good) {
			result.code = (replyWidth == 0 || replyWidth > NRF24_PHY_MAX_FRAME_BYTES)
							  ? TxCode::BAD_REPLY_SIZE
							  : TxCode::IO_ERROR;
		} else {
			result.code = (replyWidth >= NRF24_PHY_MIN_FRAME_BYTES) ? TxCode::ACK_WITH_DATA
																	: TxCode::BAD_REPLY_SIZE;
		}
	}
	const bool irqCleared = radio_.clearIrq(NRF24_PHY_IRQ_CLEAR_MASK);
	if (PP_PHY_MEASURE_TIMING) {
		markReadStage(stageStarted, timing_.irqTicks);
	}
	if (!irqCleared) {
		result.code = TxCode::IO_ERROR;
	}
	return finish();
}

NrfAckPayloadPhy::RxCode NrfAckPayloadPhy::receive(
	uint8_t (&data)[NRF24_PHY_MAX_FRAME_BYTES], uint8_t& width) {
	width = 0;
	uint8_t fifo = 0;
	if (!radio_.readRegister(Nrf24::FIFO_STATUS, fifo)) {
		return RxCode::IO_ERROR;
	}
	if (fifo & NRF24_PHY_RX_FULL_MASK) {
		++rxFifoFullSamples_;
	}
	if (fifo & NRF24_PHY_RX_EMPTY_MASK) {
		return RxCode::EMPTY;
	}
	if (!radio_.readDynamicPayload(data, width)) {
		return (width == 0 || width > NRF24_PHY_MAX_FRAME_BYTES) ? RxCode::BAD_SIZE
																 : RxCode::IO_ERROR;
	}
	return width >= NRF24_PHY_MIN_FRAME_BYTES ? RxCode::GOOD : RxCode::BAD_SIZE;
}

NrfAckPayloadPhy::QueueCode NrfAckPayloadPhy::queueReply(
	const uint8_t (&data)[NRF24_PHY_MAX_FRAME_BYTES], uint8_t width) {
	if (width < NRF24_PHY_MIN_FRAME_BYTES || width > NRF24_PHY_MAX_FRAME_BYTES) {
		return QueueCode::IO_ERROR;
	}
	uint8_t fifo = 0;
	if (!radio_.readRegister(Nrf24::FIFO_STATUS, fifo)) {
		return QueueCode::IO_ERROR;
	}
	if (fifo & NRF24_PHY_TX_FULL_MASK) {
		return QueueCode::FULL;
	}
	return radio_.writeAckPayload(NRF24_PHY_ACK_PIPE, data, width) ? QueueCode::QUEUED
																   : QueueCode::IO_ERROR;
}

bool NrfAckPayloadPhy::resetPtxFifos() {
	radio_.ceLow();
	phase_ = TxPhase::IDLE;
	lastCompleted_ = clock_.now();
	haveLast_ = true;
	return radio_.flushTx() && radio_.flushRx() && radio_.clearIrq(NRF24_PHY_IRQ_CLEAR_MASK);
}

bool NrfAckPayloadPhy::replacePrxReply(
	const uint8_t (&data)[NRF24_PHY_MAX_FRAME_BYTES], uint8_t width) {
	if (width < NRF24_PHY_MIN_FRAME_BYTES || width > NRF24_PHY_MAX_FRAME_BYTES) {
		return false;
	}
	radio_.ceLow();
	delay_.us(NRF24_PHY_PRX_REKEY_GUARD_US);
	const bool ok = radio_.flushTx() && radio_.flushRx() &&
					radio_.clearIrq(NRF24_PHY_IRQ_CLEAR_MASK) &&
					radio_.writeAckPayload(NRF24_PHY_ACK_PIPE, data, width);
	radio_.ceHigh();
	delay_.us(NRF24_PHY_PRX_SETTLE_US);
	return ok;
}

bool NrfAckPayloadPhy::clearPrxFlags() {
	return radio_.clearIrq(NRF24_PHY_IRQ_CLEAR_MASK);
}

uint16_t NrfAckPayloadPhy::measuredSpan(uint32_t ticks) {
	return static_cast<uint16_t>(
		ticks > NRF24_PHY_TIMING_MAX_TICKS ? NRF24_PHY_TIMING_MAX_TICKS : ticks);
}

void NrfAckPayloadPhy::markTerminal(uint32_t now) {
	timing_.waitTicks = measuredSpan(now - phaseStarted_);
	phaseStarted_ = now;
}

void NrfAckPayloadPhy::markReadStage(uint32_t& started, uint16_t& elapsedTicks) {
	const uint32_t now = clock_.now();
	elapsedTicks = measuredSpan(now - started);
	started = now;
}

NrfAckPayloadPhy::TxStep NrfAckPayloadPhy::finish() {
	const uint32_t now = clock_.now();
	if (PP_PHY_MEASURE_TIMING) {
		timing_.readTicks = measuredSpan(now - phaseStarted_);
	}
	lastCompleted_ = now;
	haveLast_ = true;
	phase_ = TxPhase::IDLE;
	return TxStep::COMPLETE;
}

NrfAckPayloadPhy::TxStep NrfAckPayloadPhy::finishError(TxResult& result) {
	result.code = TxCode::IO_ERROR;
	result.retries = 0;
	(void)radio_.flushTx();
	(void)radio_.clearIrq(NRF24_PHY_IRQ_CLEAR_MASK);
	return finish();
}
