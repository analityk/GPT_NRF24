#include "../GPT_NRF2401/pp_short_probe.h"
#include <assert.h>

#define PP_SHORT_TEST_WRAP_START 65530U
#define PP_SHORT_TEST_WRAP_STOP 6U
#define PP_SHORT_TEST_WRAP_TICKS 12U
#define PP_SHORT_TEST_SAMPLE_TICKS 200U
#define PP_SHORT_TEST_SAMPLE_US 100U
#define PP_SHORT_TEST_SAMPLE_LOW_TICKS 40U
#define PP_SHORT_TEST_SAMPLE_HIGH_TICKS 400U
#define PP_SHORT_TEST_LOW_US 20U
#define PP_SHORT_TEST_HIGH_US 200U

struct FakeShortClock {
	uint16_t ticks;
	explicit FakeShortClock(uint16_t value):ticks(value){
	}

	uint16_t counter16() const{
		return ticks;
	}
};

int main(){
	FakeShortClock clock(PP_SHORT_TEST_WRAP_START);
	PpShortSourceTrace<FakeShortClock> trace(clock);
	assert(!trace.sampled());
	clock.ticks=PP_SHORT_TEST_WRAP_STOP;
	trace.sourceReady();
	assert(trace.sampled());
	assert(trace.sourceTicks()==PP_SHORT_TEST_WRAP_TICKS);
	trace.frameEncoded();
	assert(trace.encodeTicks()==0);

	PpShortProbeWindow window;
	PpShortProbeWindow::Snapshot out={};
	assert(!window.sample(out));
	for(uint16_t i=0;i<PP_SHORT_PROBE_MIN_SAMPLES;++i){
		window.observe(PP_SHORT_TEST_SAMPLE_TICKS);
	}
	assert(window.sample(out));
	assert(out.count==PP_SHORT_PROBE_MIN_SAMPLES);
	assert(out.meanUs==PP_SHORT_TEST_SAMPLE_US);
	assert(out.minUs==PP_SHORT_TEST_SAMPLE_US);
	assert(out.maxUs==PP_SHORT_TEST_SAMPLE_US);

	window.observe(PP_SHORT_TEST_SAMPLE_LOW_TICKS);
	window.observe(PP_SHORT_TEST_SAMPLE_HIGH_TICKS);
	assert(window.sample(out));
	assert(out.minUs==PP_SHORT_TEST_LOW_US);
	assert(out.maxUs==PP_SHORT_TEST_HIGH_US);

	window.clear();
	assert(!window.sample(out));
	for(uint16_t i=0;i<PP_SHORT_PROBE_MAX_SAMPLES;++i){
		window.observe(PP_SHORT_TEST_SAMPLE_TICKS);
	}
	window.observe(PP_SHORT_TEST_SAMPLE_HIGH_TICKS);
	assert(window.sample(out));
	assert(out.count==PP_SHORT_PROBE_MAX_SAMPLES);
	assert(out.meanUs==PP_SHORT_TEST_SAMPLE_US);
}
