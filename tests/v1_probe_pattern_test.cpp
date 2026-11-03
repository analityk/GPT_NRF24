#include "../GPT_NRF2401/v1_probe_pattern.h"
#include <cassert>
#include <cstdint>

int main() {
    for (uint16_t n=0; n<256; ++n) {
        for (uint8_t src=0; src<2; ++src) {
            const uint8_t dst=static_cast<uint8_t>(1U-src);
            uint8_t raw[FrameV1::FRAME_SIZE]={};
            assert(V1ProbePattern::make(src,dst,static_cast<uint8_t>(n),
                                        0xE2,raw)==FrameV1::Error::OK);
            FrameV1::View view;
            assert(FrameV1::decode(raw,view)==FrameV1::Error::OK);
            assert(view.header.src==src && view.header.dst==dst);
            assert(view.header.txSeq==n && view.header.rxLast==0xE2);
            assert(view.header.length==
                   V1ProbePattern::length(static_cast<uint8_t>(n),src));
            assert(V1ProbePattern::inspect(view,src,dst));
            assert(!V1ProbePattern::inspect(view,dst,src));
            for (uint8_t i=view.header.length; i<24; ++i)
                assert(raw[8+i]==0);
            for (uint8_t i=0; i<view.header.length; ++i) {
                raw[8+i]^=1;
                assert(FrameV1::decode(raw,view)==FrameV1::Error::OK);
                assert(!V1ProbePattern::inspect(view,src,dst));
                raw[8+i]^=1;
            }
        }
    }
    assert(V1ProbePattern::length(0,0)==0);
    assert(V1ProbePattern::length(1,0)==1);
    assert(V1ProbePattern::length(2,0)==23);
    assert(V1ProbePattern::length(3,0)==24);
    assert(V1ProbePattern::length(2,1)==0);
    assert(V1ProbePattern::length(3,1)==1);
}
