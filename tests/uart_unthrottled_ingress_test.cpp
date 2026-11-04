#include "../GPT_NRF2401/uart_reliable_stream.h"
#include "../GPT_NRF2401/byte_ring.h"
#include <cassert>
#include <cstdint>
#include <cstring>

// Reusable offered-load regression for the REAL UART boundary.
// A producer with no RTS/CTS sends even when the RX ring is full:
// the excess is irrecoverably dropped BEFORE UartReliableStream::read().
// The simulated PHY faithfully pipelines B's ACK Payload one RF
// exchange behind the A->B packet; both RF channels carry user bytes.
struct UnthrottledUart {
    ByteRing<128> rx,tx;
    uint32_t offered=0,dropped=0,emitted=0;
    void receiveByte(uint8_t value) {
        ++offered;
        if(!rx.push(value))++dropped;
    }
    bool read(uint8_t& value){return rx.pop(value);}
    uint8_t available() const{return rx.available();}
    uint8_t freeSpace() const{return tx.freeSpace();}
    bool writeAll(const uint8_t* bytes,uint8_t count) {
        if(count && !bytes)return false;
        if(tx.freeSpace()<count)return false;
        for(uint8_t i=0;i<count;++i)assert(tx.push(bytes[i]));
        return true;
    }
    // Simulate the independent UART TX interrupt draining bytes.
    void transmit(uint8_t capacity) {
        uint8_t value=0;
        for(uint8_t i=0;i<capacity && tx.pop(value);++i)++emitted;
    }
};
struct Peer {
    UnthrottledUart uart;
    PairedLink link;
    UartReliableStream<UnthrottledUart> transport;
    uint8_t packet[32],width;
    explicit Peer(bool leader):
        link(leader,0xC0DE),transport(uart),width(0) {
        transport.beginSession(0xC0DE);
    }
    void make(){assert(transport.make(link,packet,width));}
    void take(const uint8_t* bytes,uint8_t n) {
        PairedFrame::View frame;
        assert(PairedFrame::decode(bytes,n,frame)==PairedFrame::Error::OK);
        if(link.receive(frame)==PairedLink::Receipt::NEW_FRAME)
            (void)transport.accept(frame);
    }
};
struct Result {
    uint32_t inputA,inputB,lostA,lostB,forwardedA,forwardedB;
};
static Result run(uint8_t rxBytesPerExchange,uint16_t exchangeCount) {
    Peer a(true),b(false);
    b.make();
    uint8_t queued[32],queueWidth=b.width;
    std::memcpy(queued,b.packet,queueWidth);
    for(uint16_t tick=0;tick<exchangeCount;++tick) {
        // RealTerm/CH340 do NOT check the AVR's free RX capacity.
        for(uint8_t i=0;i<rxBytesPerExchange;++i) {
            a.uart.receiveByte(static_cast<uint8_t>(tick+i));
            b.uart.receiveByte(static_cast<uint8_t>(tick+3U*i));
        }
        a.make();
        b.take(a.packet,a.width);
        a.take(queued,queueWidth);
        b.make();
        queueWidth=b.width;
        std::memcpy(queued,b.packet,queueWidth);
        // 19 bytes per exchange is about the throughput of UART 115200
        // when there are roughly 613 RF exchanges each second.
        a.uart.transmit(19);
        b.uart.transmit(19);
    }
    Result r={a.uart.offered,b.uart.offered,a.uart.dropped,b.uart.dropped,
              a.uart.emitted,b.uart.emitted};
    return r;
}
int main() {
    // A previous 4096B stress test pushed only <=3 bytes/exchange AND
    // stopped pushing when full, masking the unthrottled UART boundary.
    const Result paced=run(3,600);
    assert(paced.lostA==0 && paced.lostB==0);
    assert(paced.forwardedA>0 && paced.forwardedB>0);

    // Supply full-rate UART to both pins. The currently implemented
    // stop-and-wait (window=1) cannot keep up; overflow is deterministic
    // even with PERFECT RF and no CPU delays beyond each exchange.
    const Result flooded=run(19,600);
    assert(flooded.inputA==11400 && flooded.inputB==11400);
    assert(flooded.lostA>0 && flooded.lostB>0);
    assert(flooded.forwardedA<flooded.inputB &&
           flooded.forwardedB<flooded.inputA);
}
