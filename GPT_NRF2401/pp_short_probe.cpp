#ifndef GPT_HOST_TEST
#include "timer1_clock.h"
#endif
#include "pp_short_probe.h"

ShortSpanTimer::ShortSpanTimer(ClockHandle clock)
	: clock_(clock),
	  begin_(clock_.counter16()),
	  elapsed_(0),
	  done_(false) {
}

void ShortSpanTimer::stop() {
	elapsed_ = clock_.counter16() - begin_;
	done_ = true;
}

bool ShortSpanTimer::done() const {
	return done_;
}

uint16_t ShortSpanTimer::elapsedTicks() const {
	return elapsed_;
}

PpShortSourceTrace::operator TracePort() {
	TracePort port = {};
	port.context = this;
	port.sourceReadyFn = [](void* context) -> void {
		(void)context;
		static_cast<PpShortSourceTrace*>(context)->sourceReady();
	};
	port.frameEncodedFn = [](void* context) -> void {
		(void)context;
		static_cast<PpShortSourceTrace*>(context)->frameEncoded();
	};
	return port;
}

PpShortSourceTrace::PpShortSourceTrace(ClockHandle clock)
	: timer_(clock) {
}

void PpShortSourceTrace::sourceReady() {
	timer_.stop();
}

void PpShortSourceTrace::frameEncoded() {
}

bool PpShortSourceTrace::sampled() const {
	return timer_.done();
}

uint16_t PpShortSourceTrace::sourceTicks() const {
	return timer_.elapsedTicks();
}

uint16_t PpShortSourceTrace::encodeTicks() const {
	return 0;
}

PpShortProbeWindow::PpShortProbeWindow()
	: samples_(0),
	  sumTicks_(0),
	  minTicks_(UINT16_MAX),
	  maxTicks_(0) {
}

void PpShortProbeWindow::observe(uint16_t elapsedTicks) {
	if (samples_ >= PP_SHORT_PROBE_MAX_SAMPLES) {
		return;
	}
	++samples_;
	sumTicks_ += elapsedTicks;
	if (elapsedTicks < minTicks_) {
		minTicks_ = elapsedTicks;
	}
	if (elapsedTicks > maxTicks_) {
		maxTicks_ = elapsedTicks;
	}
}

bool PpShortProbeWindow::sample(Snapshot& result) const {
	if (samples_ < PP_SHORT_PROBE_MIN_SAMPLES) {
		return false;
	}
	result.count = samples_;
	result.meanUs = sumTicks_ / (static_cast<uint32_t>(samples_) * PP_SHORT_PROBE_TICKS_PER_US);
	result.minUs = minTicks_ / PP_SHORT_PROBE_TICKS_PER_US;
	result.maxUs = maxTicks_ / PP_SHORT_PROBE_TICKS_PER_US;
	return true;
}

void PpShortProbeWindow::clear() {
	samples_ = 0;
	sumTicks_ = 0;
	minTicks_ = UINT16_MAX;
	maxTicks_ = 0;
}
