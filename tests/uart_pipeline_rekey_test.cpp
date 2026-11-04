#include "../GPT_NRF2401/uart_reliable_stream.h"
#include "../GPT_NRF2401/byte_ring.h"
#include <cassert>
#include <cstdint>
#include <cstring>

// Regression for contiguous pipelining, gap recovery and rekey with a
// NONZERO former transmit base and multiple live unacknowledged segments.
// A test of in-memory transport state, not a model of RF time or UART ISR.
struct Sink {
    ByteRing<128> input,output;
    bool read(uint8_t& out){return input.pop(out);}
    uint8_t available() const{return input.available();}
    uint8_t freeSpace() const{return output.freeSpace();}
    bool writeAll(const uint8_t* bytes,uint8_t count){
        if(count && !bytes)return false;
        if(output.freeSpace()<count)return false;
        for(uint8_t i=0;i<count;++i)assert(output.push(bytes[i]));
        return true;
    }
    void push(const uint8_t* bytes,uint8_t n){
        for(uint8_t i=0;i<n;++i)assert(input.push(bytes[i]));
    }
    void drain(const uint8_t* expected,uint8_t n){
        uint8_t v=0;
        for(uint8_t i=0;i<n;++i) {
            assert(output.pop(v));
            assert(v==expected[i]);
        }
    }
};
struct Node {
    Sink serial;
    PairedLink link;
    UartReliableStream<Sink> stream;
    uint8_t bytes[32],width;
    explicit Node(bool a):link(a,0),stream(serial),width(0){}
    void resetSession(uint16_t sid) {
        link.configure(true,sid);
        stream.beginSession(sid);
    }
    void make(){assert(stream.make(link,bytes,width));}
    void take(const uint8_t* frame,uint8_t n){
        PairedFrame::View v;
        assert(PairedFrame::decode(frame,n,v)==PairedFrame::Error::OK);
        if(link.observeStream(v)!=PairedLink::Receipt::WRONG_SESSION)
            (void)stream.accept(v);
    }
};
static void preflight(Node& a,Node& b,uint16_t sid) {
    a.resetSession(sid);b.resetSession(sid);
    a.make();b.take(a.bytes,a.width);
    b.make();a.take(b.bytes,b.width);
}
// Simple full exchange with B's previously queued ACK; never pretend
// to generate a reply AFTER observing the triggering A packet.
static void exchange(Node& a,Node& b) {
    b.make();
    uint8_t reply[32];
    const uint8_t width=b.width;
    std::memcpy(reply,b.bytes,width);
    a.make();
    b.take(a.bytes,a.width);
    a.take(reply,width);
}
int main() {
    Node a(true),b(false);
    preflight(a,b,0x5101);
    uint8_t first[44];
    for(uint8_t i=0;i<44;++i)first[i]=i;
    a.serial.push(first,44);
    for(unsigned i=0;i<8;++i)exchange(a,b);
    assert(a.stream.outstanding()==0);
    assert(a.stream.acknowledgedBytes()==44);
    b.serial.drain(first,44);
    // RX_CREDIT is a delayed on-wire advertisement. Refresh it explicitly
    // after consuming the prior output before starting a new test burst.
    b.make();
    a.take(b.bytes,b.width);

    // The physical ring now starts with slot index 2. Window=4 will put
    // outstanding frames in slots 2,3,0. Rekey must preserve this order.
    uint8_t pending[66];
    for(uint8_t i=0;i<66;++i)pending[i]=static_cast<uint8_t>(100+i);
    a.serial.push(pending,66);
    for(unsigned i=0;i<3;++i)a.make(); // do NOT deliver those frames
    assert(a.stream.outstanding()==3);

    a.stream.disconnect();
    a.link.configure(true,0);
    a.resetSession(0x5102);
    b.resetSession(0x5102);
    // Do NOT use preflight() here: its synthetic ACK would already
    // acknowledge the first replayed segment before it is inspected.
    assert(a.stream.outstanding()==3);
    // The first replay must be the OLDEST pending bytes, regardless of
    // the old head index or TX_SEQ modulo256.
    a.make();
    PairedFrame::View f;
    assert(PairedFrame::decode(a.bytes,a.width,f)==PairedFrame::Error::OK);
    // All 24 BODY bytes are user data, with DATA_SEQ and ACK in header.
    assert(f.length==24 && f.header.txSeq==0);
    assert(std::memcmp(f.body,pending,24)==0);
    b.take(a.bytes,a.width);
    a.make();b.take(a.bytes,a.width);
    a.make();b.take(a.bytes,a.width);
    b.serial.drain(pending,66);
    for(unsigned i=0;i<8;++i)exchange(a,b);
    assert(a.stream.outstanding()==0);
    assert(a.stream.acknowledgedBytes()==110);

    // Inject a dropped FIRST frame, then deliver later frames. Receiver
    // must neither accept an out-of-order segment nor acknowledge a gap.
    Node c(true),d(false);preflight(c,d,0x5500);
    uint8_t sequence[88];
    for(uint8_t i=0;i<88;++i)sequence[i]=static_cast<uint8_t>(i*29U);
    c.serial.push(sequence,88);
    c.make(); // discard, never entered D's UART queue
    assert(c.stream.outstanding()==1);
    for(unsigned i=0;i<3;++i) {
        c.make();
        d.take(c.bytes,c.width);
    }
    assert(d.stream.acceptedBytes()==0 && d.stream.badFrames()>0);
    for(unsigned i=0;i<64 && d.stream.acceptedBytes()!=88;++i)
        exchange(c,d);
    assert(d.stream.acceptedBytes()==88);
    d.serial.drain(sequence,88);
    for(unsigned i=0;i<12;++i)exchange(c,d);
    assert(c.stream.outstanding()==0);
    assert(c.stream.retransmits()>0);
}
