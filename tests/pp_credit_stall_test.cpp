#include "../GPT_NRF2401/uart_reliable_stream.h"
#include "../GPT_NRF2401/byte_ring.h"
#include <cassert>

struct CreditPort {
    ByteRing<128> input,output;
    uint8_t available()const{return input.available();}
    uint8_t freeSpace()const{return output.freeSpace();}
    bool read(uint8_t& value){return input.pop(value);}
    bool writeAll(const uint8_t* data,uint8_t n){
        if(output.freeSpace()<n)return false;
        for(uint8_t i=0;i<n;++i)assert(output.push(data[i]));
        return true;
    }
};
static void accept(PairedLink& link,UartReliableStream<CreditPort>& stream,
                   const uint8_t* packet,uint8_t width){
    PairedFrame::View f;
    assert(PairedFrame::decode(packet,width,f)==PairedFrame::Error::OK);
    assert(link.observeStream(f)!=PairedLink::Receipt::WRONG_SESSION);
    stream.accept(f);
}
int main(){
    CreditPort aPort,bPort;
    PairedLink aLink(true,0x3456),bLink(false,0x3456);
    UartReliableStream<CreditPort> a(aPort),b(bPort);
    a.beginSession(0x3456);b.beginSession(0x3456);
    uint8_t frame[32],width=0,reply[32],replyWidth=0;
    // Receiver first advertises four whole packet slots.
    assert(bLink.makeStream(0,0xFF,4,0,0,reply,replyWidth)
           ==PairedFrame::Error::OK);
    accept(aLink,a,reply,replyWidth);
    for(uint8_t i=0;i<96;++i)assert(aPort.input.push(i));
    for(uint8_t i=0;i<4;++i){
        assert(a.make(aLink,frame,width));
        assert(width==32);
        // Hold all four on the sender side, e.g. ACKs lost or delayed.
    }
    assert(a.outstanding()==4);

    // The peer runs out of local application storage. Even if every one
    // of the 4 outstanding packets appears unacknowledged, a replay must
    // not be sent while RX_CREDIT=0. This is NOT an RF retransmission
    // failure: the receiver currently cannot take any new DATA.
    // A NACK may arrive in the same ACK payload that advertises zero
    // space. It schedules a replay, but must not bypass flow control.
    assert(bLink.makeStream(0,0xFF,0,0,0,reply,replyWidth,true)
           ==PairedFrame::Error::OK);
    accept(aLink,a,reply,replyWidth);
    for(uint8_t i=0;i<50;++i){
        assert(a.make(aLink,frame,width));
        PairedFrame::View f;
        assert(PairedFrame::decode(frame,width,f)==PairedFrame::Error::OK);
        assert(f.header.kind()==PairedFrame::Kind::IDLE);
    }
    assert(a.retransmits()==0 && a.outstanding()==4);

    // Buffer pressure subsides; retry can now fill the missing prefix.
    assert(bLink.makeStream(0,0xFF,4,0,0,reply,replyWidth)
           ==PairedFrame::Error::OK);
    accept(aLink,a,reply,replyWidth);
    for(uint8_t i=0;i<64 && a.outstanding()!=0;++i){
        assert(a.make(aLink,frame,width));
        accept(bLink,b,frame,width);
        assert(b.make(bLink,reply,replyWidth));
        accept(aLink,a,reply,replyWidth);
    }
    assert(a.outstanding()==0);
    assert(a.retransmits()>=4);
    assert(bPort.output.available()==96);
    uint8_t value=0;
    for(uint8_t i=0;i<96;++i)
        assert(bPort.output.pop(value) && value==i);
}
