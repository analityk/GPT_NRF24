#include "../GPT_NRF2401/uart_reliable_stream.h"
#include "../GPT_NRF2401/byte_ring.h"
#include <cassert>

struct TestPort {
    ByteRing<128> input,output;
    uint8_t available()const{return input.available();}
    uint8_t freeSpace()const{return output.freeSpace();}
    bool read(uint8_t& value){return input.pop(value);}
    bool writeAll(const uint8_t* bytes,uint8_t count){
        if(output.freeSpace()<count)return false;
        for(uint8_t i=0;i<count;++i)assert(output.push(bytes[i]));
        return true;
    }
};
static PairedFrame::View decode(const uint8_t* bytes,uint8_t width){
    PairedFrame::View frame;
    assert(PairedFrame::decode(bytes,width,frame)==PairedFrame::Error::OK);
    return frame;
}
static void deliver(PairedLink& link,UartReliableStream<TestPort>& stream,
                    const uint8_t* bytes,uint8_t width){
    const PairedFrame::View frame=decode(bytes,width);
    assert(link.observeStream(frame)!=PairedLink::Receipt::WRONG_SESSION);
    (void)stream.accept(frame);
}
int main(){
    TestPort txPort,rxPort;
    PairedLink txLink(true,0x9123),rxLink(false,0x9123);
    UartReliableStream<TestPort> tx(txPort),rx(rxPort);
    tx.beginSession(0x9123);rx.beginSession(0x9123);
    uint8_t packet[32]={},ack[32]={},width=0,ackWidth=0;
    assert(rx.make(rxLink,ack,ackWidth));
    deliver(txLink,tx,ack,ackWidth); // learn the real sink credit
    for(uint8_t i=0;i<96;++i)assert(txPort.input.push(i));

    // First segment arrives; second one is lost at radio/RX FIFO level.
    assert(tx.make(txLink,packet,width));
    assert(decode(packet,width).header.txSeq==0);
    deliver(rxLink,rx,packet,width);
    assert(tx.make(txLink,packet,width));
    assert(decode(packet,width).header.txSeq==1);
    // Third segment reaches the receiver BEFORE the missing second one.
    assert(tx.make(txLink,packet,width));
    assert(decode(packet,width).header.txSeq==2);
    deliver(rxLink,rx,packet,width);
    assert(rx.badFrames()==1);

    // A single bit in the existing header advertises the missing prefix.
    assert(rx.make(rxLink,ack,ackWidth));
    const PairedFrame::View nack=decode(ack,ackWidth);
    assert((nack.header.flags&PairedFrame::URGENT)!=0);
    assert(nack.header.rxLast==0);
    deliver(txLink,tx,ack,ackWidth);
    assert(tx.outstanding()==2);

    // Fast retransmission of missing DATA 1, without waiting eight turns.
    assert(tx.make(txLink,packet,width));
    assert(decode(packet,width).header.txSeq==1);
    assert(tx.retransmits()==1);
    // Several hardware-queued copies of the SAME NACK may follow.
    // They must not repeatedly restart the replay cursor.
    deliver(txLink,tx,ack,ackWidth);
    deliver(txLink,tx,ack,ackWidth);
    deliver(rxLink,rx,packet,width);
    assert(tx.make(txLink,packet,width));
    assert(decode(packet,width).header.txSeq==2);
    deliver(rxLink,rx,packet,width);

    // Receiver now has the missing contiguous prefix; NACK is withdrawn.
    assert(rx.make(rxLink,ack,ackWidth));
    assert((decode(ack,ackWidth).header.flags&PairedFrame::URGENT)==0);
    assert(decode(ack,ackWidth).header.rxLast==2);
    deliver(txLink,tx,ack,ackWidth);
    assert(tx.outstanding()==0);

    // New data still flows in correct byte order, and no loss occurred.
    assert(tx.make(txLink,packet,width));
    assert(decode(packet,width).header.txSeq==3);
    deliver(rxLink,rx,packet,width);
    assert(rx.make(rxLink,ack,ackWidth));
    deliver(txLink,tx,ack,ackWidth);
    assert(tx.outstanding()==0);
    assert(rxPort.output.available()==96);
    uint8_t byte=0;
    for(uint8_t i=0;i<96;++i)
        assert(rxPort.output.pop(byte) && byte==i);

    // The same mechanism must work after transport state is rebuilt for
    // a different peer boot, including sequence zero and ACK=0xFF.
    tx.beginSession(0x1234);rx.beginSession(0x1234);
    txLink.resetSession(0x1234);rxLink.resetSession(0x1234);
    assert(rx.make(rxLink,ack,ackWidth));
    deliver(txLink,tx,ack,ackWidth);
    for(uint8_t i=0;i<48;++i)assert(txPort.input.push(i));
    assert(tx.make(txLink,packet,width)); // lost DATA 0
    assert(tx.make(txLink,packet,width));
    assert(decode(packet,width).header.txSeq==1);
    deliver(rxLink,rx,packet,width);
    assert(rx.make(rxLink,ack,ackWidth));
    assert(decode(ack,ackWidth).header.rxLast==0xFF);
    deliver(txLink,tx,ack,ackWidth);
    assert(tx.make(txLink,packet,width));
    assert(decode(packet,width).header.txSeq==0);
}
