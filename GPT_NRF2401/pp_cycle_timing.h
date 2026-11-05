#ifndef GPT_NRF24_PP_CYCLE_TIMING_H
#define GPT_NRF24_PP_CYCLE_TIMING_H

#include <stdint.h>

// A portable, bounded measurement window. The five PHY components are
// disjoint portions of an exchange. Background work overlaps radio wait,
// whereas preparation and protocol processing are diagnostic subsets of
// the CPU time between exchanges. All inputs are 2 MHz Timer1 ticks.
#define PP_TIMING_TICKS_PER_US 2UL
#define PP_TIMING_MAX_SPAN_TICKS 0xFFFFUL
#define PP_TIMING_MAX_CYCLE_TICKS 40000UL
#define PP_TIMING_MAX_SAMPLES 1024U
#define PP_TIMING_MIN_SAMPLES 32U

class PpCycleTimingWindow {
public:
    struct Average {
        uint16_t samples;
        uint16_t cycleUs,loadUs,guardUs,ceUs,waitUs,readUs;
        uint16_t cpuUs,prepareUs,protocolUs,backgroundUs;
        Average():samples(0),cycleUs(0),loadUs(0),guardUs(0),
                  ceUs(0),waitUs(0),readUs(0),cpuUs(0),
                  prepareUs(0),protocolUs(0),backgroundUs(0){}
    };

    PpCycleTimingWindow():lastDone_(0),samples_(0),previous_(false) {
        for(uint8_t i=0;i<FIELD_COUNT;++i)sums_[i]=0;
    }

    static uint16_t clampTicks(uint32_t elapsed) {
        return static_cast<uint16_t>(elapsed>PP_TIMING_MAX_SPAN_TICKS?
                                     PP_TIMING_MAX_SPAN_TICKS:elapsed);
    }

    // The previous and current exchange both need to be full, healthy DATA
    // turns. A reset, short CONTROL, timeout or missing ACK breaks continuity
    // so that rekey time cannot be mislabeled as normal RF timing.
    template<class Phases>
    void observe(uint32_t completed,const Phases& t,uint16_t prepareTicks,
                 uint16_t protocolTicks,uint16_t backgroundTicks,
                 bool eligible) {
        if(!eligible) {previous_=false;resetSums();return;}
        if(previous_ && samples_<PP_TIMING_MAX_SAMPLES) {
            const uint32_t period=completed-lastDone_;
            if(period<=PP_TIMING_MAX_CYCLE_TICKS) {
                const uint32_t exclusive=static_cast<uint32_t>(t.loadTicks)+
                    t.guardTicks+t.ceTicks+t.waitTicks+t.readTicks;
                sums_[CYCLE]+=period;
                sums_[LOAD]+=t.loadTicks;
                sums_[GUARD]+=t.guardTicks;
                sums_[CE]+=t.ceTicks;
                sums_[WAIT]+=t.waitTicks;
                sums_[READ]+=t.readTicks;
                sums_[CPU]+=period>=exclusive?period-exclusive:0;
                sums_[PREPARE]+=prepareTicks;
                sums_[PROTOCOL]+=protocolTicks;
                sums_[BACKGROUND]+=backgroundTicks;
                ++samples_;
            }
        }
        lastDone_=completed;
        previous_=true;
    }

    // Read without consuming. A full UART TX ring may reject the line;
    // retrying later must preserve the measurements instead of losing them.
    bool sample(Average& out) const {
        if(samples_<PP_TIMING_MIN_SAMPLES)return false;
        out.samples=samples_;
        out.cycleUs=mean(CYCLE);
        out.loadUs=mean(LOAD);
        out.guardUs=mean(GUARD);
        out.ceUs=mean(CE);
        out.waitUs=mean(WAIT);
        out.readUs=mean(READ);
        out.cpuUs=mean(CPU);
        out.prepareUs=mean(PREPARE);
        out.protocolUs=mean(PROTOCOL);
        out.backgroundUs=mean(BACKGROUND);
        return true;
    }
    void clearWindow(){resetSums();}
    bool take(Average& out) {
        const bool valid=sample(out);
        resetSums();
        return valid;
    }

private:
    enum Field : uint8_t {
        CYCLE,LOAD,GUARD,CE,WAIT,READ,CPU,PREPARE,PROTOCOL,BACKGROUND,
        FIELD_COUNT
    };
    uint16_t mean(Field field) const {
        const uint32_t denominator=
            static_cast<uint32_t>(samples_)*PP_TIMING_TICKS_PER_US;
        return static_cast<uint16_t>(sums_[field]/denominator);
    }
    void resetSums() {
        samples_=0;
        for(uint8_t i=0;i<FIELD_COUNT;++i)sums_[i]=0;
    }
    uint32_t sums_[FIELD_COUNT];
    uint32_t lastDone_;
    uint16_t samples_;
    bool previous_;
};

#endif
