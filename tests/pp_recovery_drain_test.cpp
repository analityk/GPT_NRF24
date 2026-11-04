#include "../GPT_NRF2401/uart_reliable_stream.h"
#include "../GPT_NRF2401/byte_ring.h"
#include <cassert>

// A receiver gap must drain the existing bounded radio window BEFORE the
// sender admits fresh user segments. Otherwise the sender can perpetuate
// Go-back-N gaps at saturated offered load even after the RF link recovers.
struct DrainPort {
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
static PairedFrame::View view(const uint8_t (&packet)[32],uint8_t width){
    PairedFrame::View v;
    assert(PairedFrame::decode(packet,width,v)==PairedFrame::Error::OK);
    return v;
}
static void deliver(PairedLink& link,UartReliableStream<DrainPort>& stream,
                    const uint8_t (&packet)[32],uint8_t width){
    const PairedFrame::View v=view(packet,width);
    assert(link.observeStream(v)!=PairedLink::Receipt::WRONG_SESSION);
    (void)stream.accept(v);
}
int main(){
    DrainPort src,sink;
    PairedLink tx(false,0x5A31),rx(true,0x5A31);
    UartReliableStream<DrainPort> sender(src),receiver(sink);
    sender.beginSession(0x5A31);receiver.beginSession(0x5A31);
    uint8_t packet[32]={},ack[32]={},width=0,ackWidth=0;
    assert(receiver.make(rx,ack,ackWidth));
    deliver(tx,sender,ack,ackWidth);
    for(uint8_t i=0;i<120;++i)assert(src.input.push(i));

    assert(sender.make(tx,packet,width)); // seq0, received
    assert(view(packet,width).header.txSeq==0);
    deliver(rx,receiver,packet,width);
    assert(sender.make(tx,packet,width)); // seq1, lost
    assert(view(packet,width).header.txSeq==1);
    assert(sender.make(tx,packet,width)); // seq2, gap reported
    assert(view(packet,width).header.txSeq==2);
    deliver(rx,receiver,packet,width);
    assert(receiver.badFrames()==1);
    assert(receiver.make(rx,ack,ackWidth));
    assert(view(ack,ackWidth).header.flags&PairedFrame::URGENT);
    deliver(tx,sender,ack,ackWidth); // ACK0 + NACK1
    assert(sender.outstanding()==2);

    assert(sender.make(tx,packet,width)); // replay seq1
    assert(view(packet,width).header.txSeq==1);
    deliver(rx,receiver,packet,width);
    assert(sender.make(tx,packet,width)); // replay seq2
    assert(view(packet,width).header.txSeq==2);
    deliver(rx,receiver,packet,width);

    // ACK1/2 may be queued or delayed in hardware. Letting DATA3 into
    // the radio now is legal by credit but destabilizes recovery under
    // load; IDLE/ACK must keep the wire active without consuming data.
    assert(sender.make(tx,packet,width));
    assert(view(packet,width).header.kind()==PairedFrame::Kind::IDLE);
    assert(src.input.available()==48);

    assert(receiver.make(rx,ack,ackWidth));
    assert(view(ack,ackWidth).header.rxLast==2);
    assert(!(view(ack,ackWidth).header.flags&PairedFrame::URGENT));
    deliver(tx,sender,ack,ackWidth);
    assert(sender.outstanding()==0);

    // Back to healthy full-rate admission without manual w/s commands.
    for(uint8_t seq=3;seq<5;++seq){
        assert(sender.make(tx,packet,width));
        assert(view(packet,width).header.kind()==PairedFrame::Kind::DATA);
        assert(view(packet,width).header.txSeq==seq);
        deliver(rx,receiver,packet,width);
        assert(receiver.make(rx,ack,ackWidth));
        deliver(tx,sender,ack,ackWidth);
    }
    assert(sender.outstanding()==0);
    assert(receiver.badFrames()==1);
    assert(sink.output.available()==120);
    uint8_t byte=0;
    for(uint8_t i=0;i<120;++i)assert(sink.output.pop(byte)&&byte==i);
}
