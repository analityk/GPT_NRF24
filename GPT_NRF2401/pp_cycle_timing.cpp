#ifndef GPT_HOST_TEST
#include "timer1_clock.h"
#endif
#include "pp_cycle_timing.h"

PpCycleTimingWindow::EmptyBreakdown::EmptyBreakdown()
	: pollCount(0),
	  pollSpiTicks(0),
	  observeTicks(0),
	  fifoTicks(0),
	  payloadTicks(0),
	  irqTicks(0) {
}

PpCycleTimingWindow::Average::Average()
	: samples(0),
	  cycleUs(0),
	  loadUs(0),
	  guardUs(0),
	  ceUs(0),
	  waitUs(0),
	  readUs(0),
	  cpuUs(0),
	  prepareUs(0),
	  protocolUs(0),
	  protocolOverlapUs(0),
	  backgroundUs(0),
	  polls(0),
	  pollSpiUs(0),
	  observeUs(0),
	  fifoUs(0),
	  payloadUs(0),
	  irqUs(0),
	  sourceUs(0),
	  encodeUs(0),
	  readOtherUs(0),
	  prepareOtherUs(0),
	  cpuOtherUs(0) {
}

PpCycleTimingWindow::PpCycleTimingWindow()
	: lastDone_(0),
	  samples_(0),
	  previous_(false) {
	for (uint8_t i = 0; i < FIELD_COUNT; ++i) {
		sums_[i] = 0;
	}
}

uint16_t PpCycleTimingWindow::clampTicks(uint32_t elapsed) {
	return static_cast<uint16_t>(
		elapsed > PP_TIMING_MAX_SPAN_TICKS ? PP_TIMING_MAX_SPAN_TICKS : elapsed);
}

void PpCycleTimingWindow::observe(uint32_t completed, const NrfAckPayloadPhy::Timing& t,
	uint16_t prepareTicks, uint16_t protocolTicks, uint16_t backgroundTicks, bool eligible) {
	const NrfAckPayloadPhy::Timing empty;
	observeDetailed(
		completed, t, empty, prepareTicks, protocolTicks, backgroundTicks, 0, 0, 0, eligible);
}

void PpCycleTimingWindow::observe(uint32_t completed, const NrfAckPayloadPhy::Timing& t,
	uint16_t prepareTicks, uint16_t protocolTicks, uint16_t backgroundTicks, uint16_t sourceTicks,
	uint16_t encodeTicks, bool eligible) {
	observeDetailed(completed, t, t, prepareTicks, protocolTicks, backgroundTicks, sourceTicks,
		encodeTicks, 0, eligible);
}

void PpCycleTimingWindow::observe(uint32_t completed, const NrfAckPayloadPhy::Timing& t,
	uint16_t prepareTicks, uint16_t protocolTicks, uint16_t backgroundTicks, uint16_t sourceTicks,
	uint16_t encodeTicks, uint16_t protocolOverlapTicks, bool eligible) {
	observeDetailed(completed, t, t, prepareTicks, protocolTicks, backgroundTicks, sourceTicks,
		encodeTicks, protocolOverlapTicks, eligible);
}

void PpCycleTimingWindow::observeDetailed(uint32_t completed, const NrfAckPayloadPhy::Timing& t,
	const NrfAckPayloadPhy::Timing& detail, uint16_t prepareTicks, uint16_t protocolTicks,
	uint16_t backgroundTicks, uint16_t sourceTicks, uint16_t encodeTicks,
	uint16_t protocolOverlapTicks, bool eligible) {
	if (!eligible) {
		previous_ = false;
		resetSums();
		return;
	}
	if (previous_ && samples_ < PP_TIMING_MAX_SAMPLES) {
		const uint32_t period = completed - lastDone_;
		if (period <= PP_TIMING_MAX_CYCLE_TICKS) {
			const uint32_t exclusive = static_cast<uint32_t>(t.loadTicks) + t.guardTicks +
									   t.ceTicks + t.waitTicks + t.readTicks;
			sums_[CYCLE] += period;
			sums_[LOAD] += t.loadTicks;
			sums_[GUARD] += t.guardTicks;
			sums_[CE] += t.ceTicks;
			sums_[WAIT] += t.waitTicks;
			sums_[READ] += t.readTicks;
			sums_[CPU] += period >= exclusive ? period - exclusive : 0;
			sums_[PREPARE] += prepareTicks;
			sums_[PROTOCOL] += protocolTicks;
			sums_[PROTOCOL_OVERLAP] += protocolOverlapTicks;
			sums_[BACKGROUND] += backgroundTicks;
			sums_[POLL_COUNT] += detail.pollCount;
			sums_[POLL_SPI] += detail.pollSpiTicks;
			sums_[OBSERVE] += detail.observeTicks;
			sums_[FIFO] += detail.fifoTicks;
			sums_[PAYLOAD] += detail.payloadTicks;
			sums_[IRQ] += detail.irqTicks;
			sums_[SOURCE] += sourceTicks;
			sums_[ENCODE] += encodeTicks;
			++samples_;
		}
	}
	lastDone_ = completed;
	previous_ = true;
}

bool PpCycleTimingWindow::sample(Average& out) const {
	if (samples_ < PP_TIMING_MIN_SAMPLES) {
		return false;
	}
	out.samples = samples_;
	out.cycleUs = mean(CYCLE);
	out.loadUs = mean(LOAD);
	out.guardUs = mean(GUARD);
	out.ceUs = mean(CE);
	out.waitUs = mean(WAIT);
	out.readUs = mean(READ);
	out.cpuUs = mean(CPU);
	out.prepareUs = mean(PREPARE);
	out.protocolUs = mean(PROTOCOL);
	out.protocolOverlapUs = mean(PROTOCOL_OVERLAP);
	out.backgroundUs = mean(BACKGROUND);
	out.polls = meanCount(POLL_COUNT);
	out.pollSpiUs = mean(POLL_SPI);
	out.observeUs = mean(OBSERVE);
	out.fifoUs = mean(FIFO);
	out.payloadUs = mean(PAYLOAD);
	out.irqUs = mean(IRQ);
	out.sourceUs = mean(SOURCE);
	out.encodeUs = mean(ENCODE);
	out.readOtherUs = remainder(out.readUs, out.observeUs, out.fifoUs, out.payloadUs, out.irqUs);
	out.prepareOtherUs = remainder(out.prepareUs, out.sourceUs, out.encodeUs);
	out.cpuOtherUs = remainder(
		out.cpuUs, out.prepareUs, static_cast<uint16_t>(out.protocolUs - out.protocolOverlapUs));
	return true;
}

void PpCycleTimingWindow::clearWindow() {
	resetSums();
}

bool PpCycleTimingWindow::take(Average& out) {
	const bool valid = sample(out);
	resetSums();
	return valid;
}

uint16_t PpCycleTimingWindow::mean(Field field) const {
	const uint32_t denominator = static_cast<uint32_t>(samples_) * PP_TIMING_TICKS_PER_US;
	return static_cast<uint16_t>(sums_[field] / denominator);
}

uint16_t PpCycleTimingWindow::meanCount(Field field) const {
	return static_cast<uint16_t>(sums_[field] / samples_);
}

uint16_t PpCycleTimingWindow::remainder(
	uint16_t total, uint16_t a, uint16_t b, uint16_t c, uint16_t d) {
	const uint32_t subtotal = static_cast<uint32_t>(a) + b + c + d;
	return subtotal <= total ? static_cast<uint16_t>(total - subtotal) : 0;
}

void PpCycleTimingWindow::resetSums() {
	samples_ = 0;
	for (uint8_t i = 0; i < FIELD_COUNT; ++i) {
		sums_[i] = 0;
	}
}

PpFrameMakeTrace::operator TracePort() {
	TracePort port = {};
	port.context = this;
	port.sourceReadyFn = [](void* context) -> void {
		(void)context;
		static_cast<PpFrameMakeTrace*>(context)->sourceReady();
	};
	port.frameEncodedFn = [](void* context) -> void {
		(void)context;
		static_cast<PpFrameMakeTrace*>(context)->frameEncoded();
	};
	return port;
}

PpFrameMakeTrace::PpFrameMakeTrace(ClockHandle clock, uint32_t started, bool enabled)
	: enabled_(enabled),
	  clock_(clock),
	  started_(started),
	  sourceTicks_(0),
	  encodeTicks_(0) {
}

void PpFrameMakeTrace::sourceReady() {
	if (enabled_) {
		const uint32_t now = clock_.now();
		sourceTicks_ = PpCycleTimingWindow::clampTicks(now - started_);
		started_ = now;
	}
}

void PpFrameMakeTrace::frameEncoded() {
	if (enabled_) {
		encodeTicks_ = PpCycleTimingWindow::clampTicks(clock_.now() - started_);
	}
}

uint16_t PpFrameMakeTrace::sourceTicks() const {
	return sourceTicks_;
}

uint16_t PpFrameMakeTrace::encodeTicks() const {
	return encodeTicks_;
}
