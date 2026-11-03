#include "../GPT_NRF2401/rf_raw_pattern.h"
#include "../GPT_NRF2401/adc0.h"
#include <cassert>
#include <cstdint>

int main() {
    for (uint32_t n=0; n<65536; n+=257) {
        uint8_t frame[32]={};
        RawRfPattern::make(frame, static_cast<uint16_t>(n));
        uint16_t seq=0;
        assert(RawRfPattern::validate(frame,RawRfPattern::REQUEST,seq) && seq==n);
        assert(!RawRfPattern::validate(frame,RawRfPattern::REPLY,seq));
        RawRfPattern::markReply(frame);
        assert(RawRfPattern::validate(frame,RawRfPattern::REPLY,seq) && seq==n);
        for (uint8_t i=0; i<32; ++i) {
            const uint8_t old=frame[i];
            frame[i]^=1U;
            assert(!RawRfPattern::validate(frame,RawRfPattern::REPLY,seq));
            frame[i]=old;
        }
    }
    assert(Adc0::classForM1(0)==0);
    assert(Adc0::classForM1(511)==0);
    assert(Adc0::classForM1(512)==1);
    assert(Adc0::classForM1(675)==1);
    assert(Adc0::classForM1(1023)==1);
}
