#include "../GPT_NRF2401/pp_cycle_timing.h"
#include <assert.h>
#include <stdint.h>

#define PP_TIMING_OVERLAP_FRAMES (PP_TIMING_MIN_SAMPLES+1U)
#define PP_TIMING_OVERLAP_CYCLE_TICKS 3000UL
#define PP_TIMING_OVERLAP_LOAD_TICKS 40U
#define PP_TIMING_OVERLAP_GUARD_TICKS 10U
#define PP_TIMING_OVERLAP_CE_TICKS 30U
#define PP_TIMING_OVERLAP_RF_TICKS 700U
#define PP_TIMING_OVERLAP_READ_TICKS 20U
#define PP_TIMING_OVERLAP_PREPARE_TICKS 80U
#define PP_TIMING_OVERLAP_PROTOCOL_TICKS 100U
#define PP_TIMING_OVERLAP_PARALLEL_TICKS 80U
#define PP_TIMING_OVERLAP_BACKGROUND_TICKS 200U
#define PP_TIMING_OVERLAP_SOURCE_TICKS 20U
#define PP_TIMING_OVERLAP_ENCODE_TICKS 30U

struct OverlapTiming {
	uint16_t loadTicks;
	uint16_t guardTicks;
	uint16_t ceTicks;
	uint16_t waitTicks;
	uint16_t readTicks;
	uint16_t pollCount;
	uint16_t pollSpiTicks;
	uint16_t observeTicks;
	uint16_t fifoTicks;
	uint16_t payloadTicks;
	uint16_t irqTicks;

	OverlapTiming():loadTicks(PP_TIMING_OVERLAP_LOAD_TICKS),
		guardTicks(PP_TIMING_OVERLAP_GUARD_TICKS),
		ceTicks(PP_TIMING_OVERLAP_CE_TICKS),
		waitTicks(PP_TIMING_OVERLAP_RF_TICKS),
		readTicks(PP_TIMING_OVERLAP_READ_TICKS),
		pollCount(0),pollSpiTicks(0),observeTicks(0),fifoTicks(0),
		payloadTicks(0),irqTicks(0){}
};

int main(){
	PpCycleTimingWindow timing;
	OverlapTiming phases;
	uint32_t now=0;
	for(uint16_t i=0;i<PP_TIMING_OVERLAP_FRAMES;++i){
		timing.observe(now,phases,PP_TIMING_OVERLAP_PREPARE_TICKS,
			PP_TIMING_OVERLAP_PROTOCOL_TICKS,
			PP_TIMING_OVERLAP_BACKGROUND_TICKS,
			PP_TIMING_OVERLAP_SOURCE_TICKS,
			PP_TIMING_OVERLAP_ENCODE_TICKS,
			PP_TIMING_OVERLAP_PARALLEL_TICKS,true);
		now+=PP_TIMING_OVERLAP_CYCLE_TICKS;
	}
	PpCycleTimingWindow::Average averages;
	assert(timing.take(averages));
	assert(averages.protocolUs==PP_TIMING_OVERLAP_PROTOCOL_TICKS/
		PP_TIMING_TICKS_PER_US);
	assert(averages.protocolOverlapUs==PP_TIMING_OVERLAP_PARALLEL_TICKS/
		PP_TIMING_TICKS_PER_US);
	assert(averages.cpuOtherUs==averages.cpuUs-averages.prepareUs-
		(averages.protocolUs-averages.protocolOverlapUs));
}
