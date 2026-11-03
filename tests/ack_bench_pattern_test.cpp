#include "../GPT_NRF2401/ack_bench_pattern.h"
#include <cassert>
#include <cstdint>
#include <cstring>

int main() {
    uint8_t raw[32]={};
    for (uint8_t role=0;role<=1;++role) {
        for (uint32_t seq=0;seq<1024;++seq) {
            AckBenchPattern::make(role,seq,raw);
            uint32_t detected=0xFFFFFFFFUL;
            assert(AckBenchPattern::inspect(raw,role,detected));
            assert(detected==seq);
            assert(!AckBenchPattern::inspect(raw,static_cast<uint8_t>(1-role),
                                              detected));
            for (unsigned i=0;i<32;++i) {
                raw[i]^=0x20;
                assert(!AckBenchPattern::inspect(raw,role,detected));
                raw[i]^=0x20;
            }
        }
    }
    const uint32_t wraps[]={0xFFFFFFFEUL,0xFFFFFFFFUL,0UL,1UL};
    AckBenchPattern::Tracker t;
    for (unsigned i=0;i<4;++i)t.observe(wraps[i]);
    assert(t.gaps()==0 && t.duplicates()==0);
    t.observe(1);
    assert(t.duplicates()==1);
    t.observe(5);
    assert(t.gaps()==3);
    t.observe(2);
    assert(t.duplicates()==2);
}
