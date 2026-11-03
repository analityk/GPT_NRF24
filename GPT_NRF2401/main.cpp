/*
 * Paired diagnostic RF core, 8 B header + up to 24 B OPAQUE data.
 * Both boards run one binary; A0 low => A/PTX, high => B/PRX.
 *
 * Physical ACK payload provides the return direction with no role change.
 * 16-bit SESSION_ID below is a deliberate test fixture, not pairing!
 * Periodic UART output is DIAGNOSTIC only; the user payload never enters it.
 * Single active paired-core firmware. Historical variants are archived in Git.
 */
#include "board_clock.h"
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "gpio.h"
#include "uart.h"
#include "spi.h"
#include "nrf24.h"
#include "avr_delay.h"
#include "adc0.h"
#include "timer1_clock.h"
#include "paired_frame.h"
#include "paired_link.h"
#include "nrf24_ack_phy.h"

typedef Nrf24<SpiMaster,GPIO> Radio;
typedef NrfAckPayloadPhy<Radio,AvrDelay,Timer1Clock> AckPhy;
static const uint16_t TEST_SESSION=0x4E52;
static UART uart;
static SpiMaster spi;
static GPIO ce(&PORTB,PB2),csn(&PORTB,PB1);
static Radio radio(spi,ce,csn);
static AvrDelay delay;
static Timer1Clock ticks;
static AckPhy phy(radio,delay,ticks);
static PairedLink linkState(true,TEST_SESSION);
static uint8_t packet[32],incoming[32],body[24];
static uint8_t ackStaged[32],ackSize=0;
static bool ackWaiting=false;

class Diagnostic {
public:
    Diagnostic():size_(0) {}
    void word(const char* s) {while(*s) put(*s++);}
    void number(uint32_t n) {
        char rev[10];uint8_t i=0;
        do {rev[i++]=static_cast<char>('0'+n%10UL);n/=10UL;}
        while(n && i<10);
        while(i)put(rev[--i]);
    }
    void send() {
        put('\r');put('\n');
        if(uart.freeSpace()>=size_)
            (void)uart.writeAll(reinterpret_cast<const uint8_t*>(data_),size_);
        size_=0;
    }
private:
    void put(char c) {if(size_<sizeof(data_))data_[size_++]=c;}
    char data_[112];uint8_t size_;
};

struct Period {
    uint16_t tx,rx,queued,arc,maxRt,empty,bad,io,full;
    Period():tx(0),rx(0),queued(0),arc(0),maxRt(0),empty(0),
             bad(0),io(0),full(0) {}
    void reset() {*this=Period();}
};
static Period period;
static uint32_t lastSecond=0;

static void pattern(uint8_t role,uint8_t seq,uint8_t (&out)[24]) {
    for(uint8_t i=0;i<24;++i)
        out[i]=static_cast<uint8_t>(
            (static_cast<uint16_t>(seq)*37U +
             static_cast<uint16_t>(i)*41U +
             static_cast<uint16_t>(role)*71U) ^
            (static_cast<uint16_t>(seq)>>(i&3U)));
}
static bool check(const uint8_t* bytes,uint8_t role,uint8_t seq) {
    if(!bytes)return false;
    uint8_t expected[24];
    pattern(role,seq,expected);
    for(uint8_t i=0;i<24;++i)if(bytes[i]!=expected[i])return false;
    return true;
}
static void report(bool leader,PairedLink& link) {
    const uint32_t now=ticks.now();
    if(static_cast<uint32_t>(now-lastSecond)<2000000UL)return;
    lastSecond=now;
    Diagnostic l;
    l.word(leader?"A TX/s=":"B RX/s=");
    l.number(leader?period.tx:period.rx);
    l.word(leader?" ACK/s=":" Q/s=");
    l.number(leader?period.rx:period.queued);
    l.word(" ARC=");l.number(period.arc);
    l.word(" MAX=");l.number(period.maxRt);
    l.word(" BAD=");l.number(period.bad);
    l.word(" IO=");l.number(period.io);
    l.word(" GAB=");l.number(leader?link.remoteLoss():link.localLoss());
    l.word(" GBA=");l.number(leader?link.localLoss():link.remoteLoss());
    l.word(" DROP=");l.number(link.localDrops());
    l.word(" FULL=");l.number(period.full);
    l.send();
    period.reset();
}
static void fatal(const char* what) {
    Diagnostic l;l.word("PAIR CORE FAIL ");l.word(what);l.send();
    for(;;)delay.ms(100);
}
static bool makeDataOrIdle(PairedLink& link,uint8_t role,
                           uint8_t* dst,uint8_t& width) {
    if(link.peerCredit()!=0) {
        pattern(role,link.nextTxSeq(),body);
        return link.make(PairedFrame::Kind::DATA,0,1,body,24,
                         dst,width)==PairedFrame::Error::OK;
    }
    return link.make(PairedFrame::Kind::IDLE,0,1,0,0,
                     dst,width)==PairedFrame::Error::OK;
}
static void processFrame(PairedLink& link,uint8_t peer,uint8_t width) {
    PairedFrame::View view;
    if(PairedFrame::decode(incoming,width,view)!=PairedFrame::Error::OK) {
        ++period.bad;return;
    }
    const PairedLink::Receipt receipt=link.receive(view);
    if(receipt==PairedLink::Receipt::WRONG_SESSION) {
        ++period.bad;return;
    }
    if(receipt!=PairedLink::Receipt::NEW_FRAME)return;
    if(view.header.kind()==PairedFrame::Kind::DATA) {
        if(view.length!=24 || !check(view.body,peer,view.header.txSeq)) {
            ++period.bad;
            link.noteLocalDrop();
            return;
        }
    }
    ++period.rx;
}
static void runA() {
    if(!phy.beginPtx())fatal("PTX");
    delay.ms(150); // only once, allow PRX to boot
    Diagnostic hello;
    hello.word("PAIR CORE A PTX 8+24B SID16 SPI8");
    hello.send();
    lastSecond=ticks.now();
    for(;;) {
        uint8_t width=0;
        if(!makeDataOrIdle(linkState,0,packet,width))fatal("ENCODE");
        uint8_t incomingWidth=0;
        const AckPhy::TxResult tx=phy.transfer(packet,width,incoming,incomingWidth);
        linkState.notePtxRetries(tx.retries);
        period.arc=static_cast<uint16_t>(period.arc+tx.retries);
        if(tx.code==AckPhy::TxCode::ACK_WITH_DATA ||
           tx.code==AckPhy::TxCode::ACK_EMPTY) ++period.tx;
        if(tx.code==AckPhy::TxCode::ACK_WITH_DATA)
            processFrame(linkState,1,incomingWidth);
        else if(tx.code==AckPhy::TxCode::ACK_EMPTY) ++period.empty;
        else if(tx.code==AckPhy::TxCode::MAX_RT) ++period.maxRt;
        else if(tx.code==AckPhy::TxCode::BAD_REPLY_SIZE) ++period.bad;
        else if(tx.code==AckPhy::TxCode::IO_ERROR ||
                tx.code==AckPhy::TxCode::TIMEOUT) ++period.io;
        report(true,linkState);
    }
}

static bool queueB() {
    if(!ackWaiting) {
        if(!makeDataOrIdle(linkState,1,ackStaged,ackSize))return false;
        ackWaiting=true;
    }
    const AckPhy::QueueCode q=phy.queueReply(ackStaged,ackSize);
    if(q==AckPhy::QueueCode::QUEUED) {
        ackWaiting=false;
        ++period.queued;
    } else if(q==AckPhy::QueueCode::FULL) {
        ++period.full;
    } else ++period.io;
    return q==AckPhy::QueueCode::QUEUED;
}
static void runB() {
    if(!phy.beginPrx())fatal("PRX");
    // Initial idle ACK advertises one packet of buffer credit.
    // B must NOT send DATA before it knows A's advertised RX credit.
    if(!queueB())fatal("PRIME");
    Diagnostic hello;hello.word("PAIR CORE B PRX 8+24B SID16 SPI8");hello.send();
    lastSecond=ticks.now();
    for(;;) {
        uint8_t incomingWidth=0;
        const AckPhy::RxCode rx=phy.receive(incoming,incomingWidth);
        if(rx==AckPhy::RxCode::GOOD) {
            processFrame(linkState,0,incomingWidth);
            if(!phy.clearPrxFlags()) ++period.io;
            (void)queueB();
        } else if(rx==AckPhy::RxCode::BAD_SIZE) {
            ++period.bad;
            if(!phy.clearPrxFlags()) ++period.io;
        } else if(rx==AckPhy::RxCode::IO_ERROR) {
            ++period.io;
            if(!phy.clearPrxFlags()) ++period.io;
        }
        report(false,linkState);
    }
}
int main() {
    uart.begin();
    Timer1Clock::begin();
    sei();
    ce.clear();csn.set();
    delay.ms(110);
    if(radio.probeAndConfigure().result!=Radio::Result::OK)
        fatal("NRF");
    spi.setSpeed(SpiMaster::Speed::MHz8);
    if(!radio.enableAckPayload(0x23))fatal("DPL");
    uint16_t adc=0;
    if(!Adc0::sample(adc))fatal("ADC");
    const bool isA=(Adc0::classForM1(adc)==0);
    linkState.configure(isA,TEST_SESSION);
    if(isA)runA();
    else runB();
}
