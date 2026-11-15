#ifndef GPT_NRF24_ACK_PAYLOAD_PHY_H
#define GPT_NRF24_ACK_PAYLOAD_PHY_H
#include "nrf24.h"
#include "hardware_bindings.h"
#include <stdint.h>
#include "foreground_ports.h"
#ifndef PP_PHY_MEASURE_TIMING
#define PP_PHY_MEASURE_TIMING 0
#endif

// One-step-at-a-time DPL RF exchange. Reusable with a mocked Radio, Delay,
// Clock and with the real AVR adapters. No AVR headers, no heap, no UART.
// PTX transmits a packet; the reply arrives INSIDE hardware
// Enhanced ShockBurst ACK. PRX never changes its radio mode per packet.
// Timer Clock must return monotonic modulo-2^32 ticks at 2 MHz.
#define NRF24_PHY_MIN_FRAME_BYTES 8U
#define NRF24_PHY_MAX_FRAME_BYTES 32U
#define NRF24_PHY_POWERUP_MS 2U
#define NRF24_PHY_PRX_SETTLE_US 150U
#define NRF24_PHY_PRX_REKEY_GUARD_US 550U
#define NRF24_PHY_PTX_GUARD_TICKS 260UL
#define NRF24_PHY_CE_PULSE_TICKS 30UL
#define NRF24_PHY_TX_TIMEOUT_TICKS 16000UL
#define NRF24_PHY_CONFIG_PTX 0x0EU
#define NRF24_PHY_CONFIG_PRX 0x0FU
#define NRF24_PHY_IRQ_CLEAR_MASK 0x70U
#define NRF24_PHY_IRQ_TX_DONE_MASK 0x20U
#define NRF24_PHY_IRQ_MAX_RT_MASK 0x10U
#define NRF24_PHY_OBSERVE_RETRY_MASK 0x0FU
#define NRF24_PHY_RX_EMPTY_MASK 0x01U
#define NRF24_PHY_RX_FULL_MASK 0x02U
#define NRF24_PHY_TX_FULL_MASK 0x20U
#define NRF24_PHY_ACK_PIPE 0U
#define NRF24_PHY_TIMING_MAX_TICKS 0xFFFFUL

class NrfAckPayloadPhy {
  public:
	enum class TxCode : uint8_t {
		ACK_WITH_DATA,
		ACK_EMPTY,
		BAD_REPLY_SIZE,
		MAX_RT,
		TIMEOUT,
		IO_ERROR
	};
	enum class RxCode : uint8_t { EMPTY, GOOD, BAD_SIZE, IO_ERROR };
	enum class QueueCode : uint8_t { QUEUED, FULL, IO_ERROR };
	enum class TxStep : uint8_t { IDLE, PENDING, COMPLETE };
	enum class TxPhase : uint8_t { IDLE, GUARD, CE_PULSE, ON_AIR };

	struct TxResult {
		TxCode code;
		uint8_t retries;
		TxResult();
	};

	struct Timing {
		uint16_t loadTicks, guardTicks, ceTicks, waitTicks, readTicks;
		// ON_AIR status polling is part of waitTicks; these counts and
		// durations must never be added to the total exchange period.
		uint16_t pollCount, pollSpiTicks;
		// Disjoint portions of readTicks, from the terminal IRQ onward.
		uint16_t observeTicks, fifoTicks, payloadTicks, irqTicks;
		Timing();
	};

	NrfAckPayloadPhy(RadioHandle radio, DelayHandle delay, ClockHandle clock);

	// Number of receive polls where the existing FIFO_STATUS read saw all
	// three hardware RX slots occupied. This is NOT a count of dropped
	// packets: the chip has no direct software-visible RX-overflow counter.
	uint16_t rxFifoFullSamples() const;
	TxPhase txPhase() const;
	bool txBusy() const;
	bool txOnAir() const;
	const Timing& lastTiming() const;
	uint32_t lastCompletedAt() const;

	// Power up exactly once. Subsequent TX operations remain in PTX and
	// incur no 1.5/2 ms oscillator start penalties.
	bool beginPtx();
	bool beginPrx();

	// Submit one complete frame to the radio's FIFO. No copy of the 32B
	// payload is kept in PHY; the caller may reuse its buffer immediately.
	// GUARD and CE_PULSE are driven by pollTransfer(), never by busy waits.
	bool startTransfer(const uint8_t (&out)[NRF24_PHY_MAX_FRAME_BYTES], uint8_t txWidth);

	// At most one status SPI transaction per call; reads the reply as soon
	// as TX_DS/MAX_RT appears. The radio runs during the caller's other
	// work. replyWidth comes from hardware DPL, not a payload LEN field.
	// This method MUST be serviced frequently: CE needs >=15us high, and
	// the receiver's ACK is pulled out before a new frame can start.
	TxStep pollTransfer(
		uint8_t (&reply)[NRF24_PHY_MAX_FRAME_BYTES], uint8_t& replyWidth, TxResult& result);

	RxCode receive(uint8_t (&data)[NRF24_PHY_MAX_FRAME_BYTES], uint8_t& width);

	QueueCode queueReply(const uint8_t (&data)[NRF24_PHY_MAX_FRAME_BYTES], uint8_t width);

	// Discard stale FIFO content on an epoch transition. The PTX keeps its
	// role and oscillator running; there is no repeated power-up penalty.
	bool resetPtxFifos();

	// ACK Payload is prepared BEFORE the matching incoming packet. When
	// changing the session or response opcode, a queued old ACK must NOT
	// survive into the next epoch. Complete the possible 32B on-air ACK
	// (130us turnaround + ~329us frame) before flushing in standby.
	// The brief PRX blackout is permitted at a handshake boundary only.
	bool replacePrxReply(const uint8_t (&data)[NRF24_PHY_MAX_FRAME_BYTES], uint8_t width);

	bool clearPrxFlags();

  private:
	static uint16_t measuredSpan(uint32_t ticks);
	void markTerminal(uint32_t now);
	void markReadStage(uint32_t& started, uint16_t& elapsedTicks);
	TxStep finish();
	TxStep finishError(TxResult& result);
	RadioHandle radio_;
	DelayHandle delay_;
	ClockHandle clock_;
	uint32_t lastCompleted_;
	uint32_t phaseStarted_;
	bool haveLast_;
	uint16_t rxFifoFullSamples_;
	TxPhase phase_;
	Timing timing_;
};
#endif
