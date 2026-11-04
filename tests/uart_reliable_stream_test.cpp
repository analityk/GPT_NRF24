#include "../GPT_NRF2401/uart_reliable_stream.h"
#include "../GPT_NRF2401/byte_ring.h"
#include <cassert>
#include <cstdint>
#include <cstring>

struct Port {
    ByteRing<128> in,out;
    bool read(uint8_t& b) {return in.pop(b);}
    uint8_t available() const {return in.available();}
    uint8_t freeSpace() const {return out.freeSpace();}
    bool writeAll(const uint8_t* p,uint8_t n) {
        if(n && !p)return false;
        if(out.freeSpace()<n)return false;
        for(uint8_t i=0;i<n;++i)assert(out.push(p[i]));
        return true;
    }
    void input(uint8_t v) {assert(in.push(v));}
};
struct Node {
    Port port;
    PairedLink link;
    UartReliableStream<Port> stream;
    uint8_t raw[32];
    uint8_t width;
    explicit Node(bool a):link(a,0),stream(port),width(0){}
    void session(uint16_t sid){
        link.configure(true,sid);
        stream.beginSession(sid);
    }
    void make(){assert(stream.make(link,raw,width));}
    bool incoming(const Node& other) {
        PairedFrame::View f;
        assert(PairedFrame::decode(other.raw,other.width,f)==PairedFrame::Error::OK);
        const PairedLink::Receipt r=link.observeStream(f);
        return r!=PairedLink::Receipt::WRONG_SESSION && stream.accept(f);
    }
};
static void connect(Node& a,Node& b,uint16_t sid) {
    a.session(sid);b.session(sid);
    // Make the initial credit visible to both sides, including ACK pipelining.
    a.make();assert(!b.incoming(a));
    b.make();assert(!a.incoming(b));
}
static void exchange(Node& a,Node& b) {
    // B's ACK is PREPARED before A's current frame arrives.
    b.make();
    const uint8_t back[32]={};
    uint8_t snapshot[32];(void)back;
    std::memcpy(snapshot,b.raw,b.width);
    const uint8_t backWidth=b.width;
    a.make();
    (void)b.incoming(a);
    PairedFrame::View v;
    assert(PairedFrame::decode(snapshot,backWidth,v)==PairedFrame::Error::OK);
    if(a.link.observeStream(v)!=PairedLink::Receipt::WRONG_SESSION)
        (void)a.stream.accept(v);
}
static void drain(Port& p,const uint8_t* expected,unsigned n) {
    uint8_t value=0;
    for(unsigned i=0;i<n;++i) {
        assert(p.out.pop(value));
        assert(value==expected[i]);
    }
    assert(!p.out.pop(value));
}
int main() {
    Node a(true),b(false);
    connect(a,b,0x1234);
    assert(a.stream.credit()==5 && b.stream.credit()==5);
    uint8_t ab[24],ba[24];
    for(uint8_t i=0;i<24;++i){
        ab[i]=static_cast<uint8_t>(i==0?0:(i==1?255:i*13U));
        ba[i]=static_cast<uint8_t>(i==0?255:(i==1?0:255U-i*11U));
        a.port.input(ab[i]);b.port.input(ba[i]);
    }
    // Data in both directions, with multiple delayed ACK-payload replies.
    for(unsigned i=0;i<10;++i)exchange(a,b);
    drain(b.port,ab,24);drain(a.port,ba,24);
    assert(a.stream.acknowledgedBytes()==24);
    assert(b.stream.acknowledgedBytes()==24);
    assert(a.stream.acceptedBytes()==24);
    assert(b.stream.acceptedBytes()==24);
    // The next DATA packet has 1 octet of raw BODY, no transport header.
    a.port.input(0xA5);
    a.make();assert(a.width==9);
    PairedFrame::View direct;
    assert(PairedFrame::decode(a.raw,a.width,direct)==PairedFrame::Error::OK);
    assert(direct.length==1 && direct.body[0]==0xA5);
    assert(direct.header.txSeq==1 && direct.header.rxLast==0);

    // Unacknowledged segment has to be REPEATED, not discarded after
    // hardware ACK. Resending may happen with a different RF TX_SEQ.
    Node c(true),d(false);connect(c,d,0x5656);
    c.port.input(0x42);
    c.make();(void)d.incoming(c);
    assert(d.stream.acceptedBytes()==1);
    // Until the ACK arrives we send short STATUS frames; the next user
    // DATA can be placed into another window slot, not blocked.
    c.make();(void)d.incoming(c);
    assert(d.stream.acceptedBytes()==1 && d.stream.duplicates()==0);
    // The next frame has the cumulative application ACK.
    d.make();(void)c.incoming(d);
    assert(c.stream.acknowledgedBytes()==1 && !c.stream.hasPending());

    // A repeated DATA segment may carry a NEW ACK for the reverse stream.
    // MAC must not discard the whole frame merely because TX_SEQ repeated.
    Node dupA(true),dupB(false);connect(dupA,dupB,0xACDC);
    dupA.port.input('X');dupB.port.input('Y');
    dupA.make();(void)dupB.incoming(dupA);
    dupB.make();(void)dupA.incoming(dupB);
    assert(dupB.stream.outstanding()==1);
    uint8_t repeated[32],rw=0;
    const uint8_t same='X';
    assert(dupA.link.makeStream(0,0,1,&same,1,repeated,rw)
           ==PairedFrame::Error::OK);
    PairedFrame::View repeatedView;
    assert(PairedFrame::decode(repeated,rw,repeatedView)
           ==PairedFrame::Error::OK);
    assert(dupB.link.observeStream(repeatedView)
           ==PairedLink::Receipt::DUPLICATE);
    (void)dupB.stream.accept(repeatedView);
    assert(dupB.stream.acknowledgedBytes()==1);
    assert(!dupB.stream.hasPending());
    assert(dupB.stream.acceptedBytes()==1);
    assert(dupB.stream.duplicates()==1);
    drain(dupA.port,reinterpret_cast<const uint8_t*>("Y"),1);
    drain(dupB.port,reinterpret_cast<const uint8_t*>("X"),1);

    // Full sink: no acknowledgement and no partial write until enough
    // output room is released.
    Node x(true),y(false);connect(x,y,0x7878);
    for(unsigned i=0;i<122;++i)assert(y.port.out.push(0x55));
    for(unsigned i=0;i<24;++i)x.port.input(static_cast<uint8_t>(i));
    x.make();(void)y.incoming(x);
    assert(y.stream.acceptedBytes()==0 && y.stream.blocked()==1);
    for(unsigned i=0;i<24;++i){
        uint8_t dummy;
        assert(y.port.out.pop(dummy));
    }
    // No premature ACK; eventually the bounded Go-back-N timeout
    // resends exactly the same segment after sink headroom is restored.
    for(unsigned i=0;i<16 && y.stream.acceptedBytes()==0;++i) {
        x.make();(void)y.incoming(x);
    }
    assert(y.stream.acceptedBytes()==24);
    assert(y.port.out.available()==122);
    // Clear old sentinel then check the precise payload.
    for(unsigned i=0;i<98;++i) {
        uint8_t v;
        assert(y.port.out.pop(v) && v==0x55);
    }
    uint8_t expected[24];for(uint8_t i=0;i<24;++i)expected[i]=i;
    drain(y.port,expected,24);

    // Cross-session reset: keep pending bytes in the transmitting MCU,
    // but reset the two independent 8-bit sequence spaces.
    Node e(true),f(false);connect(e,f,0x1001);
    e.port.input('Q');
    e.make();
    assert(e.stream.hasPending());
    e.stream.disconnect();
    e.link.configure(true,0);
    e.session(0x1002);f.session(0x1002);
    e.make();
    PairedFrame::View v;
    assert(PairedFrame::decode(e.raw,e.width,v)==PairedFrame::Error::OK);
    assert(v.header.txSeq==0 && v.length==1 && v.body[0]=='Q');
    (void)f.incoming(e);
    drain(f.port,reinterpret_cast<const uint8_t*>("Q"),1);
    // Idle payload is only the 8B MAC header, carrying the live ACK.
    f.make();assert(f.width==8);
    assert(PairedFrame::decode(f.raw,f.width,v)==PairedFrame::Error::OK);
    assert(v.length==0 && v.header.kind()==PairedFrame::Kind::IDLE);
    assert(v.header.rxLast==0);

    // 300 segments cross the transport 8-bit wrap with no duplicates.
    Node p(true),q(false);connect(p,q,0x9999);
    for(unsigned k=0;k<300;++k) {
        p.port.input(static_cast<uint8_t>(k));
        for(unsigned n=0;n<7;++n)exchange(p,q);
        uint8_t byte=0;
        assert(q.port.out.pop(byte) && byte==static_cast<uint8_t>(k));
    }
    assert(p.stream.acknowledgedBytes()==300);
    assert(q.stream.acceptedBytes()==300);
}
