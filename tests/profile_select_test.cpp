#include "../GPT_NRF2401/profile_select.h"
#include <cassert>
#include <stdint.h>
struct Pin {
    uint8_t sequence[2],idx;
    bool pullup=false;
    Pin(uint8_t a,uint8_t b):sequence{a,b},idx(0){}
    void i2c_set(){pullup=true;}
    uint8_t read(){assert(pullup);return sequence[idx++];}
};
struct Delay {
    unsigned calls=0,elapsed=0;
    void us(unsigned n){++calls;elapsed+=n;}
};
int main(){
    assert(ProfileSelect::fromStableLevel(true)==ProfileSelect::Profile::PP);
    assert(ProfileSelect::fromStableLevel(false)==ProfileSelect::Profile::BURST20);
    Pin pp(1,1),burst(0,0),glitch(0,1);
    Delay d0,d1,d2;
    assert(ProfileSelect::latch(pp,d0)==ProfileSelect::Profile::PP);
    assert(ProfileSelect::latch(burst,d1)==ProfileSelect::Profile::BURST20);
    assert(ProfileSelect::latch(glitch,d2)==ProfileSelect::Profile::PP);
    assert(d0.elapsed==100 && d1.elapsed==100 && d2.elapsed==100);
    assert(d0.calls==2 && d1.calls==2 && d2.calls==2);
}
