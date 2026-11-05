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
    // Old callers may supply only the five high-level PHY phases.
    struct EmptyBreakdown {
        uint16_t pollCount,pollSpiTicks,observeTicks,fifoTicks,
                 payloadTicks,irqTicks;
        EmptyBreakdown():pollCount(0),pollSpiTicks(0),observeTicks(0),
                         fifoTicks(0),payloadTicks(0),irqTicks(0){}
    };
public:
    struct Average {
        uint16_t samples;
        uint16_t cycleUs,loadUs,guardUs,ceUs,waitUs,readUs;
        uint16_t cpuUs,prepareUs,protocolUs,protocolOverlapUs,backgroundUs;
        uint16_t polls,pollSpiUs,observeUs,fifoUs,payloadUs,irqUs;
        uint16_t sourceUs,encodeUs,readOtherUs,prepareOtherUs,cpuOtherUs;
		Average():samples(0),cycleUs(0),loadUs(0),guardUs(0),
			ceUs(0),waitUs(0),readUs(0),cpuUs(0),prepareUs(0),
			protocolUs(0),protocolOverlapUs(0),backgroundUs(0),
			polls(0),pollSpiUs(0),observeUs(0),fifoUs(0),
			payloadUs(0),irqUs(0),sourceUs(0),encodeUs(0),
			readOtherUs(0),prepareOtherUs(0),cpuOtherUs(0){
		}
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
        uint16_t protocolTicks,uint16_t backgroundTicks,bool eligible){
		const EmptyBreakdown empty;
		observeDetailed(completed,t,empty,prepareTicks,protocolTicks,
			backgroundTicks,0,0,0,eligible);
	}

    template<class Phases>
    void observe(uint32_t completed,const Phases& t,uint16_t prepareTicks,
        uint16_t protocolTicks,uint16_t backgroundTicks,
        uint16_t sourceTicks,uint16_t encodeTicks,bool eligible){
		observeDetailed(completed,t,t,prepareTicks,protocolTicks,
			backgroundTicks,sourceTicks,encodeTicks,0,eligible);
	}

    // Protocol handling of the previous ACK can run while the current
    // RF frame is on air. These ticks are a subset of protocolTicks AND
    // the RF wait, so they are not part of exclusive CPU idle time.
    template<class Phases>
    void observe(uint32_t completed,const Phases& t,uint16_t prepareTicks,
        uint16_t protocolTicks,uint16_t backgroundTicks,
        uint16_t sourceTicks,uint16_t encodeTicks,
        uint16_t protocolOverlapTicks,bool eligible){
		observeDetailed(completed,t,t,prepareTicks,protocolTicks,
			backgroundTicks,sourceTicks,encodeTicks,
			protocolOverlapTicks,eligible);
	}

    template<class Phases,class Detail>
    void observeDetailed(uint32_t completed,const Phases& t,
        const Detail& detail,uint16_t prepareTicks,uint16_t protocolTicks,
        uint16_t backgroundTicks,uint16_t sourceTicks,uint16_t encodeTicks,
        uint16_t protocolOverlapTicks,bool eligible){
		if(!eligible){
			previous_=false;
			resetSums();
			return;
		}
		if(previous_ && samples_<PP_TIMING_MAX_SAMPLES){
			const uint32_t period=completed-lastDone_;
			if(period<=PP_TIMING_MAX_CYCLE_TICKS){
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
				sums_[PROTOCOL_OVERLAP]+=protocolOverlapTicks;
				sums_[BACKGROUND]+=backgroundTicks;
				sums_[POLL_COUNT]+=detail.pollCount;
				sums_[POLL_SPI]+=detail.pollSpiTicks;
				sums_[OBSERVE]+=detail.observeTicks;
				sums_[FIFO]+=detail.fifoTicks;
				sums_[PAYLOAD]+=detail.payloadTicks;
				sums_[IRQ]+=detail.irqTicks;
				sums_[SOURCE]+=sourceTicks;
				sums_[ENCODE]+=encodeTicks;
				++samples_;
			}
		}
		lastDone_=completed;
		previous_=true;
	}

    // Read without consuming. A full UART TX ring may reject the line;
    // retrying later must preserve the measurements instead of losing them.
    bool sample(Average& out) const{
		if(samples_<PP_TIMING_MIN_SAMPLES){
			return false;
		}
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
		out.protocolOverlapUs=mean(PROTOCOL_OVERLAP);
		out.backgroundUs=mean(BACKGROUND);
		out.polls=meanCount(POLL_COUNT);
		out.pollSpiUs=mean(POLL_SPI);
		out.observeUs=mean(OBSERVE);
		out.fifoUs=mean(FIFO);
		out.payloadUs=mean(PAYLOAD);
		out.irqUs=mean(IRQ);
		out.sourceUs=mean(SOURCE);
		out.encodeUs=mean(ENCODE);
		out.readOtherUs=remainder(out.readUs,out.observeUs,out.fifoUs,
			out.payloadUs,out.irqUs);
		out.prepareOtherUs=remainder(out.prepareUs,out.sourceUs,
			out.encodeUs);
		out.cpuOtherUs=remainder(out.cpuUs,out.prepareUs,
			static_cast<uint16_t>(out.protocolUs-out.protocolOverlapUs));
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
        CYCLE,LOAD,GUARD,CE,WAIT,READ,CPU,PREPARE,PROTOCOL,
        PROTOCOL_OVERLAP,BACKGROUND,
        POLL_COUNT,POLL_SPI,OBSERVE,FIFO,PAYLOAD,IRQ,SOURCE,ENCODE,
        FIELD_COUNT
    };
    uint16_t mean(Field field) const {
        const uint32_t denominator=
            static_cast<uint32_t>(samples_)*PP_TIMING_TICKS_PER_US;
        return static_cast<uint16_t>(sums_[field]/denominator);
    }
    uint16_t meanCount(Field field)const {
        return static_cast<uint16_t>(sums_[field]/samples_);
    }
    static uint16_t remainder(uint16_t total,uint16_t a,uint16_t b,
                              uint16_t c=0,uint16_t d=0) {
        const uint32_t subtotal=static_cast<uint32_t>(a)+b+c+d;
        return subtotal<=total?
            static_cast<uint16_t>(total-subtotal):0;
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

// Reusable optional timer adapter for UartReliableStream::makeTraced().
// Disabled probes compile to empty callbacks and do not access the clock.
template<class Clock,bool Enabled>
class PpFrameMakeTrace {
public:
    PpFrameMakeTrace(Clock& clock,uint32_t started)
        :clock_(clock),started_(started),sourceTicks_(0),encodeTicks_(0){}
    void sourceReady() {
        if(Enabled) {
            const uint32_t now=clock_.now();
            sourceTicks_=PpCycleTimingWindow::clampTicks(now-started_);
            started_=now;
        }
    }
    void frameEncoded() {
        if(Enabled)
            encodeTicks_=PpCycleTimingWindow::clampTicks(
                clock_.now()-started_);
    }
    uint16_t sourceTicks()const{return sourceTicks_;}
    uint16_t encodeTicks()const{return encodeTicks_;}
private:
    Clock& clock_;
    uint32_t started_;
    uint16_t sourceTicks_,encodeTicks_;
};

#endif
