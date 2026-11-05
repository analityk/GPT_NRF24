#include "../GPT_NRF2401/uart_reliable_stream.h"
#include "../GPT_NRF2401/byte_ring.h"
#include "../GPT_NRF2401/retained_byte_ring.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>

// Reusable offered-load regression for the REAL UART boundary.
// A producer with no RTS/CTS sends even when the retained UART ring is full:
// excess bytes are irrecoverably dropped before application-level ACK.
// The transport shares the *same* ring with its simulated RX ISR.
// The simulated PHY faithfully pipelines B's ACK Payload one RF
// exchange behind the A->B packet; both RF channels carry user bytes.
struct UnthrottledUart {
    RetainedByteRing rx;
    ByteRing<128> tx;
    RetainedByteRing& retainedInput(){
        return rx;
    }
    uint32_t offered=0,dropped=0,emitted=0;
    void receiveByte(uint8_t value) {
        ++offered;
        if(!rx.push(value))++dropped;
    }
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
    UartReliableStream<UnthrottledUart,true> transport;
    uint8_t packet[32],width;
    explicit Peer(bool leader):
        link(leader,0xC0DE),transport(uart),width(0) {
        transport.beginSession(0xC0DE);
    }
    void make(){assert(transport.make(link,packet,width));}
    void take(const uint8_t* bytes,uint8_t n) {
        PairedFrame::View frame;
        assert(PairedFrame::decode(bytes,n,frame)==PairedFrame::Error::OK);
        if(link.observeStream(frame)!=PairedLink::Receipt::WRONG_SESSION)
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

    // Window=4 pipelines 24B segments. Model 19B/exchange at UART115200
    // near ~613 RF turns/s; 30B/exchange exceeds RF capacity.
    const Result streaming=run(19,600);
    assert(streaming.inputA==11400 && streaming.inputB==11400);
    assert(streaming.lostA==0 && streaming.lostB==0);
    const Result flooded=run(30,600);
    assert(flooded.inputA==18000 && flooded.inputB==18000);
    assert(flooded.lostA>0 && flooded.lostB>0);
    std::printf("UART offered-load model: paced(3/exchange) drops A=%lu B=%lu, "
                "streaming(19/exchange) drops A=%lu B=%lu, "
                "flooded(30/exchange) offered A=%lu B=%lu, "
                "drops A=%lu B=%lu, UART TX emitted A=%lu B=%lu\\n",
                static_cast<unsigned long>(paced.lostA),
                static_cast<unsigned long>(paced.lostB),
                static_cast<unsigned long>(streaming.lostA),
                static_cast<unsigned long>(streaming.lostB),
                static_cast<unsigned long>(flooded.inputA),
                static_cast<unsigned long>(flooded.inputB),
                static_cast<unsigned long>(flooded.lostA),
                static_cast<unsigned long>(flooded.lostB),
                static_cast<unsigned long>(flooded.forwardedA),
                static_cast<unsigned long>(flooded.forwardedB));
    assert(flooded.forwardedA<flooded.inputB &&
           flooded.forwardedB<flooded.inputA);
}
