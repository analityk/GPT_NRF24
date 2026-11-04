#include "../GPT_NRF2401/uart_reliable_stream.h"
#include "../GPT_NRF2401/byte_ring.h"
#include <cassert>
#include <cstdint>
#include <cstring>

// Long, bidirectional, binary stream simulation. Maintains an actual
// one-frame delayed PRX hardware ACK queue, including loss of the forward
// frame and loss of the ACK. Exercises UART backpressure and RF sequence
// rollovers, not merely an isolated encode/decode API.
struct Port {
    ByteRing<128> input,output;
    bool read(uint8_t& b){return input.pop(b);}
    uint8_t available()const{return input.available();}
    uint8_t freeSpace()const{return output.freeSpace();}
    bool writeAll(const uint8_t* bytes,uint8_t count){
        if(count && !bytes)return false;
        if(output.freeSpace()<count)return false;
        for(uint8_t i=0;i<count;++i)assert(output.push(bytes[i]));
        return true;
    }
};
struct Peer {
    Port serial;
    PairedLink link;
    UartReliableStream<Port> transport;
    uint8_t frame[32];
    uint8_t width;
    explicit Peer(bool a):link(a,0x4242),transport(serial),width(0){
        transport.beginSession(0x4242);
    }
    void prepare(){
        assert(transport.make(link,frame,width));
    }
    void receive(const uint8_t* bytes,uint8_t n){
        PairedFrame::View v;
        assert(PairedFrame::decode(bytes,n,v)==PairedFrame::Error::OK);
        if(link.observeStream(v)!=PairedLink::Receipt::WRONG_SESSION)
            (void)transport.accept(v);
    }
};

int main(){
    Peer a(true),b(false);
    static const unsigned COUNT=4096;
    uint8_t ab[COUNT],ba[COUNT],outAB[COUNT],outBA[COUNT];
    for(unsigned i=0;i<COUNT;++i){
        ab[i]=static_cast<uint8_t>(
            i==0?0:(i==1?255:(i*37U+i/19U)&255U));
        ba[i]=static_cast<uint8_t>(
            i==0?255:(i==1?0:(i*11U+i/23U)^0xA5U));
    }
    unsigned fedA=0,fedB=0,gotAB=0,gotBA=0;
    uint8_t ackQueue[32]={};
    uint8_t ackWidth=0;
    b.prepare();
    ackWidth=b.width;
    std::memcpy(ackQueue,b.frame,ackWidth);

    for(unsigned t=0;t<30000;++t){
        // Real 127B UART input queue, paced by the host for the simulation.
        for(unsigned i=0;i<3 && fedA<COUNT && a.serial.input.freeSpace();++i)
            assert(a.serial.input.push(ab[fedA++]));
        for(unsigned i=0;i<3 && fedB<COUNT && b.serial.input.freeSpace();++i)
            assert(b.serial.input.push(ba[fedB++]));

        a.prepare();
        const bool forwardDelivered=(t%17)!=0 && (t%43)!=0;
        const bool ackDelivered=forwardDelivered && (t%13)!=0;
        if(forwardDelivered){
            b.receive(a.frame,a.width);
            if(ackDelivered)a.receive(ackQueue,ackWidth);
            // Only now can the AVR B prepare its NEXT hardware ACK payload.
            b.prepare();
            ackWidth=b.width;
            std::memcpy(ackQueue,b.frame,ackWidth);
        }

        // Real UART consumer: drain a few bytes each exchange.
        for(unsigned i=0;i<2 && b.serial.output.available();++i){
            uint8_t value=0;assert(b.serial.output.pop(value));
            assert(gotAB<COUNT);outAB[gotAB++]=value;
        }
        for(unsigned i=0;i<2 && a.serial.output.available();++i){
            uint8_t value=0;assert(a.serial.output.pop(value));
            assert(gotBA<COUNT);outBA[gotBA++]=value;
        }

        if(gotAB==COUNT && gotBA==COUNT &&
           !a.transport.hasPending() && !b.transport.hasPending())
            break;
    }
    assert(fedA==COUNT && fedB==COUNT);
    assert(gotAB==COUNT && gotBA==COUNT);
    assert(std::memcmp(ab,outAB,COUNT)==0);
    assert(std::memcmp(ba,outBA,COUNT)==0);
    assert(!a.transport.hasPending() && !b.transport.hasPending());
    assert(a.transport.acknowledgedBytes()==COUNT &&
           b.transport.acknowledgedBytes()==COUNT);
    assert(a.transport.acceptedBytes()==COUNT &&
           b.transport.acceptedBytes()==COUNT);
    // Loss triggers bounded Go-back-N retransmissions. A recovered
    // missing packet may arrive without any duplicate reaching the sink.
    assert(a.transport.retransmits()>0 || b.transport.retransmits()>0);
    // Application accepted bytes exactly once despite RF losses.
}
