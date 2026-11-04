#include "../GPT_NRF2401/pp_echo_ramp_port.h"
#include "../GPT_NRF2401/uart_reliable_stream.h"
#include <cassert>
#include <cstdint>
#include <cstring>

struct MockConsole {
    ByteRing<128> rx,tx;
    bool read(uint8_t& value){return rx.pop(value);}
    uint8_t freeSpace()const{return tx.freeSpace();}
    bool writeAll(const uint8_t* data,uint8_t count) {
        if(count && !data)return false;
        if(tx.freeSpace()<count)return false;
        for(uint8_t i=0;i<count;++i)assert(tx.push(data[i]));
        return true;
    }
    void command(char c){assert(rx.push(static_cast<uint8_t>(c)));}
};

static void timerAndBounds() {
    MockConsole serial;
    PpEchoRampPort<MockConsole> a(serial);
    a.configure(true,0);
    a.service(19999UL);assert(a.available()==0);
    a.service(20000UL);assert(a.available()==1);
    uint8_t value=0;
    assert(a.read(value) && value==0);
    a.service(40000UL);assert(a.read(value) && value==1);

    serial.command('s');a.service(40000UL);
    assert(a.periodUs()==9900U);
    a.service(59799UL);assert(a.available()==0);
    a.service(59800UL);assert(a.read(value) && value==2);
    serial.command('w');a.service(59800UL);
    assert(a.periodUs()==10000U);
    a.service(79800UL);assert(a.read(value) && value==3);

    // 100 commands take the interval from 10ms to zero, then saturate.
    for(unsigned i=0;i<100;++i)serial.command('s');
    a.service(79800UL);
    assert(a.periodUs()==0);
    assert(a.available()==120 && a.generated()==124);
    assert(a.pressureEvents()>0);
    for(unsigned i=0;i<120;++i) {
        assert(a.read(value));
        assert(value==static_cast<uint8_t>(i+4U));
    }
    a.service(79801UL);
    for(unsigned i=120;i<240;++i) {
        assert(a.read(value));
        assert(value==static_cast<uint8_t>(i+4U));
    }
    // A partial drain makes room without dropping or skipping the pattern.
    a.service(79802UL);
    for(unsigned i=240;i<260;++i) {
        assert(a.read(value));
        assert(value==static_cast<uint8_t>(i+4U));
    }
    assert(a.available()==100);

    // The upper bound is exactly 10ms even after excess 'w' commands.
    for(unsigned i=0;i<110;++i)serial.command('w');
    a.service(100000UL);
    assert(a.periodUs()==10000U);
    serial.command('x');a.service(100001UL);
    assert(a.periodUs()==10000U);

    // uint32 Timer1 wrap must not turn a future deadline into "due".
    a.configure(true,0xFFFFFF00UL);
    a.service(0x00004D1FUL);assert(a.available()==0);
    a.service(0x00004D20UL);assert(a.available()==1);
}

static void mirrorAndAtomicSink() {
    MockConsole serial;
    PpEchoRampPort<MockConsole> b(serial);
    b.configure(false,0);
    uint8_t data[24];
    for(uint8_t i=0;i<24;++i)data[i]=i;
    for(unsigned i=0;i<5;++i)assert(b.writeAll(data,24));
    assert(b.available()==120 && b.freeSpace()==7);
    assert(!b.writeAll(data,24) && b.available()==120);
    uint8_t value=0;
    for(unsigned j=0;j<5;++j)
        for(uint8_t i=0;i<24;++i) {
            assert(b.read(value));
            assert(value==i);
        }
    assert(b.available()==0);
    b.service(999999UL);assert(b.available()==0); // B has no generator

    PpEchoRampPort<MockConsole> a(serial);
    a.configure(true,0);
    assert(a.writeAll(data,24));
    for(uint8_t i=0;i<24;++i) {
        assert(serial.tx.pop(value));
        assert(value==i);
    }
    assert(a.echoed()==24 && a.echoGaps()==0);
    const uint8_t broken[3]={24,26,27};
    assert(a.writeAll(broken,3));
    assert(a.echoGaps()==1); // one omitted byte, then re-synchronize
}

struct Peer {
    MockConsole serial;
    PpEchoRampPort<MockConsole> port;
    PairedLink link;
    UartReliableStream<PpEchoRampPort<MockConsole> > transport;
    uint8_t frame[32],width;
    explicit Peer(bool leader)
        :port(serial),link(leader,0x7171),transport(port),width(0) {
        port.configure(leader,0);
        transport.beginSession(0x7171);
    }
    void prepare(){assert(transport.make(link,frame,width));}
    void receive(const uint8_t* bytes,uint8_t size) {
        PairedFrame::View f;
        assert(PairedFrame::decode(bytes,size,f)==PairedFrame::Error::OK);
        if(link.observeStream(f)!=PairedLink::Receipt::WRONG_SESSION)
            (void)transport.accept(f);
    }
};

static void completeRadioEcho() {
    Peer a(true),b(false);
    // At 500us per byte source input is much faster than the 10ms start.
    for(unsigned i=0;i<95;++i)a.serial.command('s');
    b.prepare();
    uint8_t reply[32];uint8_t replySize=b.width;
    std::memcpy(reply,b.frame,replySize);
    unsigned received=0;
    for(uint32_t turn=1;turn<=2500;++turn) {
        a.port.service(turn*3200UL); // 1.6ms full RF exchange model
        a.prepare();
        b.receive(a.frame,a.width);
        a.receive(reply,replySize); // preloaded ACK Payload, one turn old
        b.prepare();
        replySize=b.width;
        std::memcpy(reply,b.frame,replySize);
        uint8_t value=0;
        for(uint8_t i=0;i<19 && a.serial.tx.pop(value);++i) {
            assert(value==static_cast<uint8_t>(received));
            ++received;
        }
    }
    assert(a.port.periodUs()==500U);
    assert(received>1000U); // multiple wraps of the 0..255 pattern
    assert(a.port.echoGaps()==0);
    assert(a.transport.badFrames()==0 && b.transport.badFrames()==0);
}

int main() {
    timerAndBounds();
    mirrorAndAtomicSink();
    completeRadioEcho();
}
