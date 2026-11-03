#include "../GPT_NRF2401/uart_transport_v1.h"
#include <cassert>
#include <cstring>

struct Port {
    unsigned inHead=0,inTail=0,outLen=0;
    uint8_t input[8192]={},output[8192]={};
    unsigned capacity=8192;
    void feed(const uint8_t* d,unsigned n) {
        assert(inTail+n<=8192);
        for(unsigned i=0;i<n;++i) input[inTail++]=d[i];
    }
    bool read(uint8_t& x) {
        if(inHead>=inTail) return false;
        x=input[inHead++];
        return true;
    }
    uint8_t freeSpace() const {
        unsigned left=capacity-outLen;
        return static_cast<uint8_t>(left>127?127:left);
    }
    bool writeAll(const uint8_t* p,uint8_t n) {
        if(n>freeSpace()) return false;
        for(unsigned i=0;i<n;++i) output[outLen++]=p[i];
        return true;
    }
};

int main() {
    Port a,b;
    uint8_t dataA[1000],dataB[1000];
    for(unsigned i=0;i<1000;++i) {
        dataA[i]=static_cast<uint8_t>((i*197)^0xFF);
        dataB[i]=static_cast<uint8_t>((i*13)^0x55);
    }
    a.feed(dataA,1000);
    b.feed(dataB,1000);
    ReliableUartV1<Port> left,right;
    uint8_t raw[32]={},body[24];
    FrameV1::Header header;
    for(unsigned cycle=0;cycle<500;++cycle) {
        assert(left.prepare(a,0,1,0x2A,static_cast<uint8_t>(cycle),
                            0,header,body));
        const bool ackA=(header.kind==FrameV1::Kind::CONTROL);
        const bool dataA=(header.kind==FrameV1::Kind::DATA && header.length>0)||
                         (ackA && body[0]==0x12);
        assert(FrameV1::encode(header,body,raw)==FrameV1::Error::OK);
        FrameV1::View view;
        assert(FrameV1::decode(raw,view)==FrameV1::Error::OK);
        const bool passedA=(cycle%11!=5);
        if(passedA) right.accept(view,b);
        left.afterTransmit(passedA,ackA,dataA);

        if(passedA) {
            assert(right.prepare(b,1,0,0x2A,static_cast<uint8_t>(cycle),
                                 0,header,body));
            const bool ackB=(header.kind==FrameV1::Kind::CONTROL);
            const bool dataB=(header.kind==FrameV1::Kind::DATA && header.length>0)||
                             (ackB && body[0]==0x12);
            assert(FrameV1::encode(header,body,raw)==FrameV1::Error::OK);
            assert(FrameV1::decode(raw,view)==FrameV1::Error::OK);
            const bool passedB=(cycle%13!=7);
            if(passedB) left.accept(view,a);
            right.afterTransmit(passedB,ackB,dataB);
        }
        if(a.outLen==1000 && b.outLen==1000 &&
           !left.hasOutstanding() && !right.hasOutstanding())
            break;
        assert(!left.needsRecovery() && !right.needsRecovery());
    }
    assert(a.outLen==1000 && b.outLen==1000);
    assert(!std::memcmp(a.output,dataB,1000));
    assert(!std::memcmp(b.output,dataA,1000));
    assert(!left.hasOutstanding() && !right.hasOutstanding());
    assert(left.stats().duplicateRx+right.stats().duplicateRx>0);
}
