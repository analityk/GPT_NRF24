#ifndef GPT_NRF24_PP_SHORT_PROBE_H
#define GPT_NRF24_PP_SHORT_PROBE_H

#include <stdint.h>

// The hardware 16-bit timer wraps naturally. A single measured operation
// must take less than one timer rollover; longer deadlines still use
// Timer1Clock::now(). The window sum is wider than individual samples.
#define PP_SHORT_PROBE_TICKS_PER_US 2U
#define PP_SHORT_PROBE_MIN_SAMPLES 32U
#define PP_SHORT_PROBE_MAX_SAMPLES 1024U

// Reusable two-snapshot timer for short, nonblocking code sections.
template<class Clock>
class ShortSpanTimer {
public:
	explicit ShortSpanTimer(Clock& clock):
		clock_(clock),begin_(clock_.counter16()),elapsed_(0),done_(false){
	}

	void stop(){
		elapsed_=static_cast<uint16_t>(clock_.counter16()-begin_);
		done_=true;
	}

	bool done() const{
		return done_;
	}

	uint16_t elapsedTicks() const{
		return elapsed_;
	}

private:
	Clock& clock_;
	uint16_t begin_,elapsed_;
	bool done_;
};

// Adapter to the portable stream's optional sourceReady/frameEncoded hooks.
template<class Clock>
class PpShortSourceTrace {
public:
	explicit PpShortSourceTrace(Clock& clock):timer_(clock){
	}

	void sourceReady(){
		timer_.stop();
	}

	void frameEncoded(){
	}

	bool sampled() const{
		return timer_.done();
	}

	uint16_t sourceTicks() const{
		return timer_.elapsedTicks();
	}

	uint16_t encodeTicks() const{
		return 0;
	}

private:
	ShortSpanTimer<Clock> timer_;
};

// 32-bit aggregation runs once per segment; division runs only in the
// low-priority report. No 32-bit timestamp operations in the hot section.
class PpShortProbeWindow {
public:
	struct Snapshot {
		uint16_t count,meanUs,minUs,maxUs;
	};

	PpShortProbeWindow():
		samples_(0),sumTicks_(0),minTicks_(UINT16_MAX),maxTicks_(0){
	}

	void observe(uint16_t elapsedTicks){
		if(samples_>=PP_SHORT_PROBE_MAX_SAMPLES){
			return;
		}
		++samples_;
		sumTicks_+=elapsedTicks;
		if(elapsedTicks<minTicks_){
			minTicks_=elapsedTicks;
		}
		if(elapsedTicks>maxTicks_){
			maxTicks_=elapsedTicks;
		}
	}

	bool sample(Snapshot& result) const{
		if(samples_<PP_SHORT_PROBE_MIN_SAMPLES){
			return false;
		}
		result.count=samples_;
		result.meanUs=static_cast<uint16_t>(sumTicks_/
			(static_cast<uint32_t>(samples_)*PP_SHORT_PROBE_TICKS_PER_US));
		result.minUs=static_cast<uint16_t>(minTicks_/PP_SHORT_PROBE_TICKS_PER_US);
		result.maxUs=static_cast<uint16_t>(maxTicks_/PP_SHORT_PROBE_TICKS_PER_US);
		return true;
	}

	void clear(){
		samples_=0;
		sumTicks_=0;
		minTicks_=UINT16_MAX;
		maxTicks_=0;
	}

private:
	uint16_t samples_;
	uint32_t sumTicks_;
	uint16_t minTicks_,maxTicks_;
};

#endif
