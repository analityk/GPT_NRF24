#include "../GPT_NRF2401/uart_transport_v1.h"
#include <cassert>
#include <cstdint>
#include <cstring>

struct FakeUart {
    uint8_t input[512]={};
    unsigned inHead=0,inTail=0;
    uint8_t output[512]={};
    unsigned outLen=0;
    uint8_t capacity=127;
    void feed(const uint8_t* src,unsigned len) {
        assert(inTail+len<=sizeof(input));
        for (unsigned i=0;i<len;++i)input[inTail++]=src[i];
    }
    uint8_t available() const {return static_cast<uint8_t>(inTail-inHead);}
    bool read(uint8_t& x) {
        if(inHead==inTail)return false;
        x=input[inHead++];
        return true;
    }
    uint8_t freeSpace() const {return static_cast<uint8_t>(capacity-outLen);}
    bool writeAll(const uint8_t* p,uint8_t n) {
        if (n>freeSpace()) return false;
        for(unsigned i=0;i<n;++i) output[outLen++]=p[i];
        return true;
    }
    void drain() {outLen=0;}
};

template <typename Link>
static FrameV1::View make(Link& link,FakeUart& port,
                           uint8_t src,uint8_t dst,uint8_t seq,
                           uint8_t (&packet)[32],
                           bool& ack,bool& data) {
    FrameV1::Header h;
    uint8_t body[24]={};
    assert(link.prepare(port,src,dst,0x2A,seq,0xFF,h,body));
    ack=h.kind==FrameV1::Kind::CONTROL;
    data=(h.kind==FrameV1::Kind::DATA && h.length!=0) ||
         (h.kind==FrameV1::Kind::CONTROL && body[0]==0x12);
    assert(FrameV1::encode(h,body,packet)==FrameV1::Error::OK);
    FrameV1::View v;
    assert(FrameV1::decode(packet,v)==FrameV1::Error::OK);
    return v;
}

int main() {
    typedef ReliableUartV1<FakeUart> Link;
    FakeUart portA,portB;
    Link a,b;
    uint8_t bytesA[18],bytesB[18];
    for(unsigned i=0;i<18;++i) {
        bytesA[i]=static_cast<uint8_t>(i);
        bytesB[i]=static_cast<uint8_t>(255-i);
    }
    portA.feed(bytesA,18);
    portB.feed(bytesB,18);
    uint8_t wire[32]={};
    bool ack=false,data=false;
    // A DATA 18 -> B, B accepts its entire segment and prepares ACK+DATA.
    FrameV1::View v=make(a,portA,0,1,0,wire,ack,data);
    assert(!ack && data && v.header.length==19);
    b.accept(v,portB);
    assert(portB.outLen==18 && !std::memcmp(portB.output,bytesA,18));
    a.afterTransmit(true,ack,data);
    v=make(b,portB,1,0,0,wire,ack,data);
    assert(ack && data && v.body[0]==0x12 && v.header.length==24);
    a.accept(v,portA);
    b.afterTransmit(true,ack,data);
    assert(!a.hasOutstanding() && b.hasOutstanding());
    assert(portA.outLen==18 && !std::memcmp(portA.output,bytesB,18));
    // A ACK -> B, both pending segments delivered once.
    v=make(a,portA,0,1,1,wire,ack,data);
    assert(ack && !data && v.body[0]==0x10 && v.header.length==5);
    b.accept(v,portB);
    a.afterTransmit(true,ack,data);
    assert(!b.hasOutstanding());
    assert(b.stats().segmentsAccepted==1);
    assert(a.stats().segmentsAccepted==1);

    // Duplicate DATA if application ACK is lost must not duplicate UART.
    FakeUart left,right;
    Link x,y;
    const uint8_t input[4]={0,0xFF,'?',0x00};
    left.feed(input,4);
    v=make(x,left,0,1,0,wire,ack,data);
    y.accept(v,right);
    x.afterTransmit(true,ack,data);
    assert(right.outLen==4);
    v=make(y,right,1,0,0,wire,ack,data);
    assert(ack && !data); // drop this ACK before x sees it
    y.afterTransmit(true,ack,data);
    v=make(x,left,0,1,1,wire,ack,data);
    assert(data);
    y.accept(v,right);
    x.afterTransmit(true,ack,data);
    assert(right.outLen==4 && y.stats().duplicateRx==1);
    v=make(y,right,1,0,1,wire,ack,data);
    assert(v.body[2]==Link::ACCEPTED);
    x.accept(v,left);
    y.afterTransmit(true,ack,data);
    assert(!x.hasOutstanding());

    // BUSY never produces partial UART bytes or premature ACK=ACCEPTED.
    FakeUart busySender,busyReceiver;
    Link sender,receiver;
    busySender.feed(bytesA,18);
    busyReceiver.capacity=4;
    v=make(sender,busySender,0,1,0,wire,ack,data);
    receiver.accept(v,busyReceiver);
    assert(busyReceiver.outLen==0);
    sender.afterTransmit(true,ack,data);
    v=make(receiver,busyReceiver,1,0,0,wire,ack,data);
    assert(v.body[2]==Link::BUSY);
    sender.accept(v,busySender);
    receiver.afterTransmit(true,ack,data);
    assert(sender.hasOutstanding());
    busyReceiver.capacity=127;
    v=make(sender,busySender,0,1,1,wire,ack,data);
    receiver.accept(v,busyReceiver);
    sender.afterTransmit(true,ack,data);
    assert(busyReceiver.outLen==18);
    v=make(receiver,busyReceiver,1,0,1,wire,ack,data);
    assert(v.body[2]==Link::ACCEPTED);
    sender.accept(v,busySender);
    assert(!sender.hasOutstanding());

    // Old outstanding data must remain staged across a renegotiated SID.
    FakeUart retryUart;
    Link retained;
    retryUart.feed(input,4);
    v=make(retained,retryUart,0,1,0,wire,ack,data);
    retained.afterTransmit(false,ack,data);
    assert(retained.hasOutstanding());
    retained.newSession();
    v=make(retained,retryUart,0,1,0,wire,ack,data);
    assert(v.body[0]==0 && v.header.length==5);
    for(unsigned i=0;i<4;++i)assert(v.body[i+1]==input[i]);

    // Sending the same unacknowledged segment 64 times asks for recovery,
    // without dropping its bytes.
    FakeUart failUart;
    Link retries;
    failUart.feed(input,4);
    for (unsigned i=0;i<64;++i) {
        v=make(retries,failUart,0,1,static_cast<uint8_t>(i),wire,ack,data);
        retries.afterTransmit(false,ack,data);
    }
    assert(retries.needsRecovery() && retries.hasOutstanding());
    retries.newSession();
    assert(!retries.needsRecovery() && retries.hasOutstanding());
}
