#ifndef GPT_NRF24_BURST20_SCHEDULE_H
#define GPT_NRF24_BURST20_SCHEDULE_H
#include <stdint.h>

// Portable, tick-based MAC scheduler. No GPIO, SPI, UART, heap or delays.
// All 20ms periods are measured from a leader-owned reference using an
// integer number of Timer1 ticks (2MHz). A follower must first synchronize
// the anchor from a valid, CRC-protected on-air control exchange.
//
// The scheduler only grants transmission rights. The PHY must enforce
// the guard, abort/flush a pending RF transaction before the deadline,
// and switch PTX/PRX roles safely. A REPORT phase reserves a transmission
// OPPORTUNITY; the caller must send at most one report per slot.
class Burst20Schedule {
public:
    enum class Share : uint8_t { A5_B15=1, A10_B10=2, A15_B5=3 };
    enum class Owner : uint8_t { A=0, B=1 };
    enum class Phase : uint8_t { DATA=0, REPORT=1, GUARD=2 };
    enum class Error : uint8_t { NONE=0, BAD_SHARE=1, NOT_ALIGNED=2 };
    static const uint32_t TICKS_PER_US=2UL;
    static const uint32_t QUANTUM_TICKS=10000UL; // 5ms
    static const uint32_t CYCLE_TICKS=40000UL;   // 20ms
    static const uint32_t REPORT_TICKS=2000UL;   // 1ms reserved
    static const uint32_t GUARD_TICKS=600UL;     // 0.3ms reserved

    struct Position {
        Owner owner;
        Phase phase;
        uint16_t cycle;          // modulo 65536; reset by session handshake
        uint32_t slotRemaining;  // in 0.5us Timer1 ticks
        uint32_t phaseRemaining; // in 0.5us Timer1 ticks
        uint32_t cycleRemaining;
        Position():owner(Owner::A),phase(Phase::GUARD),cycle(0),
                   slotRemaining(0),phaseRemaining(0),cycleRemaining(0){}
        bool canStart(uint32_t worstCaseTicks) const {
            return (phase==Phase::DATA || phase==Phase::REPORT) &&
                   worstCaseTicks<=phaseRemaining;
        }
    };

    Burst20Schedule():anchor_(0),cycle_(0),share_(Share::A10_B10),
                      aligned_(false){}

    static bool valid(Share share) {
        return share==Share::A5_B15 ||
               share==Share::A10_B10 ||
               share==Share::A15_B5;
    }
    static uint8_t aQuanta(Share share) {
        return valid(share)?static_cast<uint8_t>(share):0U;
    }
    static uint8_t bQuanta(Share share) {
        return valid(share)?static_cast<uint8_t>(4U-aQuanta(share)):0U;
    }
    Error align(uint32_t cycleStartTicks,Share share,uint16_t cycle=0) {
        if(!valid(share))return Error::BAD_SHARE;
        anchor_=cycleStartTicks;
        cycle_=cycle;
        share_=share;
        aligned_=true;
        return Error::NONE;
    }
    void invalidate(){aligned_=false;}
    bool aligned()const{return aligned_;}
    Share share()const{return share_;}
    uint16_t cycle()const{return cycle_;}

    // At most one call per 2^32 Timer1 ticks (~35.8 min) is required to
    // disambiguate a wrapped time counter; regular packet/slot-loop calls
    // are sufficient. This is O(1) even after a long pause.
    Error advance(uint32_t nowTicks) {
        if(!aligned_)return Error::NOT_ALIGNED;
        const uint32_t elapsed=static_cast<uint32_t>(nowTicks-anchor_);
        const uint32_t jumps=elapsed/CYCLE_TICKS;
        if(jumps) {
            anchor_+=jumps*CYCLE_TICKS;
            cycle_=static_cast<uint16_t>(cycle_+jumps);
        }
        return Error::NONE;
    }
    Position position(uint32_t nowTicks)const {
        Position p;
        if(!aligned_)return p;
        // Call advance() before querying if the next 20ms boundary passed.
        const uint32_t offset=static_cast<uint32_t>(nowTicks-anchor_);
        if(offset>=CYCLE_TICKS)return p; // fail closed: no stale TX rights
        const uint32_t aTicks=static_cast<uint32_t>(
            aQuanta(share_))*QUANTUM_TICKS;
        const bool isA=(offset<aTicks);
        p.owner=isA?Owner::A:Owner::B;
        const uint32_t local=isA?offset:(offset-aTicks);
        const uint32_t slotLength=isA?aTicks:(CYCLE_TICKS-aTicks);
        const uint32_t dataEnd=slotLength-REPORT_TICKS-GUARD_TICKS;
        const uint32_t controlEnd=slotLength-GUARD_TICKS;
        p.slotRemaining=slotLength-local;
        p.cycleRemaining=CYCLE_TICKS-offset;
        p.cycle=cycle_;
        if(local<dataEnd) {
            p.phase=Phase::DATA;
            p.phaseRemaining=dataEnd-local;
        } else if(local<controlEnd) {
            p.phase=Phase::REPORT;
            p.phaseRemaining=controlEnd-local;
        } else {
            p.phase=Phase::GUARD;
            p.phaseRemaining=slotLength-local;
        }
        return p;
    }
private:
    uint32_t anchor_;
    uint16_t cycle_;
    Share share_;
    bool aligned_;
};
#endif
