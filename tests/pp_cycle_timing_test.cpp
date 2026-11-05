#include "../GPT_NRF2401/pp_cycle_timing.h"
#include <cassert>
#include <cstdint>

#define PP_TIMING_TEST_CYCLE_TICKS 3000UL
#define PP_TIMING_TEST_LOAD_TICKS 40U
#define PP_TIMING_TEST_GUARD_TICKS 10U
#define PP_TIMING_TEST_CE_TICKS 30U
#define PP_TIMING_TEST_WAIT_TICKS 700U
#define PP_TIMING_TEST_READ_TICKS 20U
#define PP_TIMING_TEST_PREPARE_TICKS 80U
#define PP_TIMING_TEST_PROTOCOL_TICKS 100U
#define PP_TIMING_TEST_BACKGROUND_TICKS 200U
#define PP_TIMING_TEST_FRAMES (PP_TIMING_MIN_SAMPLES+1U)
#define PP_TIMING_TEST_ROLLOVER_START 0xFFFFFF00UL
#define PP_TIMING_TEST_CYCLE_US (PP_TIMING_TEST_CYCLE_TICKS/PP_TIMING_TICKS_PER_US)
#define PP_TIMING_TEST_CPU_US ((PP_TIMING_TEST_CYCLE_TICKS-PP_TIMING_TEST_LOAD_TICKS-PP_TIMING_TEST_GUARD_TICKS-PP_TIMING_TEST_CE_TICKS-PP_TIMING_TEST_WAIT_TICKS-PP_TIMING_TEST_READ_TICKS)/PP_TIMING_TICKS_PER_US)

struct Phases {
    uint16_t loadTicks,guardTicks,ceTicks,waitTicks,readTicks;
    Phases():loadTicks(PP_TIMING_TEST_LOAD_TICKS),
             guardTicks(PP_TIMING_TEST_GUARD_TICKS),
             ceTicks(PP_TIMING_TEST_CE_TICKS),
             waitTicks(PP_TIMING_TEST_WAIT_TICKS),
             readTicks(PP_TIMING_TEST_READ_TICKS){}
};

int main() {
    PpCycleTimingWindow stats;
    Phases timings;
    uint32_t clock=PP_TIMING_TEST_ROLLOVER_START;
    for(uint16_t i=0;i<PP_TIMING_TEST_FRAMES;++i) {
        stats.observe(clock,timings,PP_TIMING_TEST_PREPARE_TICKS,
                      PP_TIMING_TEST_PROTOCOL_TICKS,
                      PP_TIMING_TEST_BACKGROUND_TICKS,true);
        clock+=PP_TIMING_TEST_CYCLE_TICKS;
    }
    PpCycleTimingWindow::Average avg;
    assert(stats.take(avg));
    assert(avg.samples==PP_TIMING_TEST_FRAMES-1U);
    assert(avg.cycleUs==PP_TIMING_TEST_CYCLE_US);
    assert(avg.loadUs==PP_TIMING_TEST_LOAD_TICKS/PP_TIMING_TICKS_PER_US);
    assert(avg.guardUs==PP_TIMING_TEST_GUARD_TICKS/PP_TIMING_TICKS_PER_US);
    assert(avg.ceUs==PP_TIMING_TEST_CE_TICKS/PP_TIMING_TICKS_PER_US);
    assert(avg.waitUs==PP_TIMING_TEST_WAIT_TICKS/PP_TIMING_TICKS_PER_US);
    assert(avg.readUs==PP_TIMING_TEST_READ_TICKS/PP_TIMING_TICKS_PER_US);
    assert(avg.cpuUs==PP_TIMING_TEST_CPU_US);
    assert(avg.prepareUs==PP_TIMING_TEST_PREPARE_TICKS/PP_TIMING_TICKS_PER_US);
    assert(avg.protocolUs==PP_TIMING_TEST_PROTOCOL_TICKS/PP_TIMING_TICKS_PER_US);
    assert(avg.backgroundUs==PP_TIMING_TEST_BACKGROUND_TICKS/PP_TIMING_TICKS_PER_US);

    // An unsuccessful exchange breaks the chain of comparable cycle times.
    stats.observe(clock,timings,0,0,0,false);
    clock+=PP_TIMING_TEST_CYCLE_TICKS;
    stats.observe(clock,timings,0,0,0,true);
    assert(!stats.take(avg));

    // A gap exceeding the declared measurement range must not pollute the
    // averages. Rollover-safe difference is unsigned modulo-2^32.
    clock+=PP_TIMING_MAX_CYCLE_TICKS+1U;
    stats.observe(clock,timings,0,0,0,true);
    assert(!stats.take(avg));

    assert(PpCycleTimingWindow::clampTicks(PP_TIMING_MAX_SPAN_TICKS+1U)==
           PP_TIMING_MAX_SPAN_TICKS);
}
