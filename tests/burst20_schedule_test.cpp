#include "../GPT_NRF2401/burst20_schedule.h"
#include <cassert>
#include <cstdint>
#include <climits>
typedef Burst20Schedule S;
static void check(S::Share share,unsigned aMs) {
    S s;
    assert(s.align(20000,share,1234)==S::Error::NONE);
    assert(S::aQuanta(share)==aMs/5);
    assert(S::bQuanta(share)==(20-aMs)/5);
    const uint32_t aBoundary=static_cast<uint32_t>(aMs*2000);
    const uint32_t aDataEnd=aBoundary-S::REPORT_TICKS-S::GUARD_TICKS;
    const uint32_t aControlEnd=aBoundary-S::GUARD_TICKS;
    const S::Position data=s.position(20000+aDataEnd-1);
    assert(data.owner==S::Owner::A && data.phase==S::Phase::DATA);
    assert(data.phaseRemaining==1 && data.canStart(1) && !data.canStart(2));
    const S::Position report=s.position(20000+aDataEnd);
    assert(report.phase==S::Phase::REPORT && report.phaseRemaining==S::REPORT_TICKS);
    assert(report.canStart(S::REPORT_TICKS));
    const S::Position guard=s.position(20000+aControlEnd);
    assert(guard.phase==S::Phase::GUARD && !guard.canStart(1));
    const S::Position bdata=s.position(20000+aBoundary);
    assert(bdata.owner==S::Owner::B && bdata.phase==S::Phase::DATA);
    assert(bdata.cycle==1234);
    const S::Position breport=s.position(20000+S::CYCLE_TICKS-
                                        S::REPORT_TICKS-S::GUARD_TICKS);
    assert(breport.owner==S::Owner::B && breport.phase==S::Phase::REPORT);
    const S::Position bguard=s.position(20000+S::CYCLE_TICKS-S::GUARD_TICKS);
    assert(bguard.phase==S::Phase::GUARD);
    assert(s.position(20000+S::CYCLE_TICKS).phase==S::Phase::GUARD);
    assert(s.advance(20000+S::CYCLE_TICKS)==S::Error::NONE);
    const S::Position next=s.position(20000+S::CYCLE_TICKS);
    assert(next.cycle==1235 && next.owner==S::Owner::A &&
           next.phase==S::Phase::DATA);
    assert(s.advance(20000+40*S::CYCLE_TICKS)==S::Error::NONE);
    assert(s.position(20000+40*S::CYCLE_TICKS).cycle==1274);
}
int main(){
    S unaligned;
    assert(!unaligned.aligned());
    assert(unaligned.advance(100)==S::Error::NOT_ALIGNED);
    assert(unaligned.position(100).phase==S::Phase::GUARD);
    assert(unaligned.align(0,static_cast<S::Share>(0))==S::Error::BAD_SHARE);
    check(S::Share::A5_B15,5);
    check(S::Share::A10_B10,10);
    check(S::Share::A15_B5,15);
    // 32-bit Timer1 rollover: anchor is maintained modulo 2^32.
    S wrapping;
    const uint32_t start=UINT32_MAX-15000U;
    assert(wrapping.align(start,S::Share::A10_B10)==S::Error::NONE);
    const uint32_t later=static_cast<uint32_t>(start+81000U);
    assert(wrapping.advance(later)==S::Error::NONE);
    assert(wrapping.cycle()==2);
    const S::Position pos=wrapping.position(later);
    assert(pos.owner==S::Owner::A && pos.phase==S::Phase::DATA &&
           pos.cycleRemaining==39000U);
    wrapping.invalidate();
    assert(!wrapping.position(later).canStart(1));
}
