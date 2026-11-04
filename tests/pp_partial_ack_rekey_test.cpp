#include "../GPT_NRF2401/uart_reliable_stream.h"
#include "../GPT_NRF2401/byte_ring.h"
#include <cassert>

struct RekeyPort {
    ByteRing<128> input,output;
    uint8_t available()const{return input.available();}
    uint8_t freeSpace()const{return output.freeSpace();}
    bool read(uint8_t& byte){return input.pop(byte);}
    bool writeAll(const uint8_t* bytes,uint8_t count){
        if(output.freeSpace()<count)return false;
        for(uint8_t i=0;i<count;++i)assert(output.push(bytes[i]));
        return true;
    }
};
static PairedFrame::View decode(const uint8_t (&bytes)[32],uint8_t width){
    PairedFrame::View frame;
    assert(PairedFrame::decode(bytes,width,frame)==PairedFrame::Error::OK);
    return frame;
}
static void deliver(PairedLink& link,UartReliableStream<RekeyPort>& stream,
                    const uint8_t (&bytes)[32],uint8_t width){
    const PairedFrame::View frame=decode(bytes,width);
    assert(link.observeStream(frame)!=PairedLink::Receipt::WRONG_SESSION);
    (void)stream.accept(frame);
}
int main(){
    RekeyPort bPort,aPort;
    PairedLink bLink(false,0x1111),aLink(true,0x2222);
    UartReliableStream<RekeyPort> b(bPort),a(aPort);
    b.beginSession(0x1111);
    uint8_t packet[32]={},ack[32]={},width=0,ackWidth=0;
    PairedLink oldA(true,0x1111);
    assert(oldA.makeStream(0,0xFF,5,0,0,ack,ackWidth)
           ==PairedFrame::Error::OK);
    deliver(bLink,b,ack,ackWidth);
    for(uint8_t i=0;i<120;++i)assert(bPort.input.push(i));

    // Old A dies while four B->A segments are in ACK Payload / in flight.
    for(uint8_t i=0;i<4;++i){
        assert(b.make(bLink,packet,width));
        assert(decode(packet,width).header.txSeq==i);
    }
    assert(b.outstanding()==4 && bPort.input.available()==24);

    // New A starts with an empty receiver; B preserves all old cache.
    a.beginSession(0x2222);
    b.beginSession(0x2222);
    bLink.resetSession(0x2222);
    assert(a.make(aLink,ack,ackWidth)); // credit from rebooted A
    deliver(bLink,b,ack,ackWidth);

    // B starts replay 0..3. The new A accepts only replayed seq 0.
    assert(b.make(bLink,packet,width));
    assert(decode(packet,width).header.txSeq==0);
    deliver(aLink,a,packet,width);
    assert(a.make(aLink,ack,ackWidth));
    assert(decode(ack,ackWidth).header.rxLast==0);
    deliver(bLink,b,ack,ackWidth);
    assert(b.outstanding()==3);

    // A partial cumulative ACK must not CANCEL replay of seq 1..3.
    // The previous implementation skipped them and emitted new seq 4.
    for(uint8_t expected=1;expected<4;++expected){
        assert(b.make(bLink,packet,width));
        assert(decode(packet,width).header.kind()==PairedFrame::Kind::DATA);
        assert(decode(packet,width).header.txSeq==expected);
        deliver(aLink,a,packet,width);
        assert(a.make(aLink,ack,ackWidth));
        deliver(bLink,b,ack,ackWidth);
    }
    assert(b.outstanding()==0);
    assert(b.make(bLink,packet,width));
    assert(decode(packet,width).header.txSeq==4);
    deliver(aLink,a,packet,width);
    assert(a.make(aLink,ack,ackWidth));
    deliver(bLink,b,ack,ackWidth);
    assert(b.outstanding()==0);
    assert(a.badFrames()==0);
    assert(aPort.output.available()==120);
    uint8_t byte=0;
    for(uint8_t i=0;i<120;++i)
        assert(aPort.output.pop(byte) && byte==i);
}
