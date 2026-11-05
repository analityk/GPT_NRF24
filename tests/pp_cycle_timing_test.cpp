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
#define PP_TIMING_TEST_POLL_COUNT 5U
#define PP_TIMING_TEST_POLL_SPI_TICKS 100U
#define PP_TIMING_TEST_OBSERVE_TICKS 4U
#define PP_TIMING_TEST_FIFO_TICKS 4U
#define PP_TIMING_TEST_PAYLOAD_TICKS 6U
#define PP_TIMING_TEST_IRQ_TICKS 2U
#define PP_TIMING_TEST_SOURCE_TICKS 20U
#define PP_TIMING_TEST_ENCODE_TICKS 30U

struct Phases {
    uint16_t loadTicks,guardTicks,ceTicks,waitTicks,readTicks;
    Phases():loadTicks(PP_TIMING_TEST_LOAD_TICKS),
             guardTicks(PP_TIMING_TEST_GUARD_TICKS),
             ceTicks(PP_TIMING_TEST_CE_TICKS),
             waitTicks(PP_TIMING_TEST_WAIT_TICKS),
             readTicks(PP_TIMING_TEST_READ_TICKS){}
};
struct DetailedPhases : Phases {
    uint16_t pollCount,pollSpiTicks,observeTicks,fifoTicks,
             payloadTicks,irqTicks;
    DetailedPhases():pollCount(PP_TIMING_TEST_POLL_COUNT),
                     pollSpiTicks(PP_TIMING_TEST_POLL_SPI_TICKS),
                     observeTicks(PP_TIMING_TEST_OBSERVE_TICKS),
                     fifoTicks(PP_TIMING_TEST_FIFO_TICKS),
                     payloadTicks(PP_TIMING_TEST_PAYLOAD_TICKS),
                     irqTicks(PP_TIMING_TEST_IRQ_TICKS){}
};
struct FakeClock {
    uint32_t ticks=0;
    uint32_t now()const{return ticks;}
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
    assert(avg.polls==0 && avg.observeUs==0 && avg.sourceUs==0);

    PpCycleTimingWindow detailed;
    DetailedPhases detailedPhases;
    for(uint16_t i=0;i<PP_TIMING_TEST_FRAMES;++i) {
        detailed.observe(clock,detailedPhases,PP_TIMING_TEST_PREPARE_TICKS,
                         PP_TIMING_TEST_PROTOCOL_TICKS,
                         PP_TIMING_TEST_BACKGROUND_TICKS,
                         PP_TIMING_TEST_SOURCE_TICKS,
                         PP_TIMING_TEST_ENCODE_TICKS,true);
        clock+=PP_TIMING_TEST_CYCLE_TICKS;
    }
    assert(detailed.take(avg));
    assert(avg.polls==PP_TIMING_TEST_POLL_COUNT);
    assert(avg.pollSpiUs==PP_TIMING_TEST_POLL_SPI_TICKS/PP_TIMING_TICKS_PER_US);
    assert(avg.observeUs==PP_TIMING_TEST_OBSERVE_TICKS/PP_TIMING_TICKS_PER_US);
    assert(avg.fifoUs==PP_TIMING_TEST_FIFO_TICKS/PP_TIMING_TICKS_PER_US);
    assert(avg.payloadUs==PP_TIMING_TEST_PAYLOAD_TICKS/PP_TIMING_TICKS_PER_US);
    assert(avg.irqUs==PP_TIMING_TEST_IRQ_TICKS/PP_TIMING_TICKS_PER_US);
    assert(avg.readOtherUs==(PP_TIMING_TEST_READ_TICKS-
           PP_TIMING_TEST_OBSERVE_TICKS-PP_TIMING_TEST_FIFO_TICKS-
           PP_TIMING_TEST_PAYLOAD_TICKS-PP_TIMING_TEST_IRQ_TICKS)/
           PP_TIMING_TICKS_PER_US);
    assert(avg.sourceUs==PP_TIMING_TEST_SOURCE_TICKS/PP_TIMING_TICKS_PER_US);
    assert(avg.encodeUs==PP_TIMING_TEST_ENCODE_TICKS/PP_TIMING_TICKS_PER_US);
    assert(avg.prepareOtherUs==(PP_TIMING_TEST_PREPARE_TICKS-
           PP_TIMING_TEST_SOURCE_TICKS-PP_TIMING_TEST_ENCODE_TICKS)/
           PP_TIMING_TICKS_PER_US);
    assert(avg.cpuOtherUs==avg.cpuUs-avg.prepareUs-avg.protocolUs);

    FakeClock fake;
    PpFrameMakeTrace<FakeClock,true> traced(fake,fake.now());
    fake.ticks+=PP_TIMING_TEST_SOURCE_TICKS;
    traced.sourceReady();
    fake.ticks+=PP_TIMING_TEST_ENCODE_TICKS;
    traced.frameEncoded();
    assert(traced.sourceTicks()==PP_TIMING_TEST_SOURCE_TICKS);
    assert(traced.encodeTicks()==PP_TIMING_TEST_ENCODE_TICKS);
    PpFrameMakeTrace<FakeClock,false> disabled(fake,fake.now());
    fake.ticks+=PP_TIMING_TEST_CYCLE_TICKS;
    disabled.sourceReady();disabled.frameEncoded();
    assert(disabled.sourceTicks()==0 && disabled.encodeTicks()==0);

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
