#include "application_select.h"
#if GPT_NRF24_APPLICATION == 1

/*
 * Physical ACK-payload ping-pong benchmark: 32 bytes A->B, 32 bytes B->A
 * in Enhanced ShockBurst ACK. The same firmware runs on both RF-Nano boards.
 * A0=GND => A/PTX, A0=VCC => B/PRX.
 *
 * This is NOT UART file transport. Serial 115200 8N1 carries only one
 * statistics line per second. No sleep between packet transactions.
 * Default radio 1Mbps, CRC16, auto ACK; SPI accelerated to 8MHz.
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
#include "ack_bench_pattern.h"
#include "nrf24_ack_phy.h"

typedef Nrf24<SpiMaster,GPIO> Radio;
typedef NrfAckPayloadPhy<Radio,AvrDelay,Timer1Clock> AckPhy;
static UART uart;
static SpiMaster spi;
static GPIO radioCe(&PORTB,PB2);
static GPIO radioCsn(&PORTB,PB1);
static Radio radio(spi,radioCe,radioCsn);
static AvrDelay delay;
static Timer1Clock clockTicks;
static AckPhy phy(radio,delay,clockTicks);
static uint8_t packet[32],received[32];

class BenchLine {
public:
    BenchLine():n_(0) {}
    void text(const char* s) { while (*s) chr(*s++); }
    void dec(uint32_t value) {
        char tmp[10];
        uint8_t digits=0;
        do { tmp[digits++]=static_cast<char>('0'+value%10UL);
             value/=10UL; } while (value && digits<sizeof(tmp));
        while (digits) chr(tmp[--digits]);
    }
    void send() {
        chr('\r');chr('\n');
        // Diagnostics never corrupt RF metrics by blocking on UART.
        if (uart.freeSpace()>=n_)
            (void)uart.writeAll(reinterpret_cast<const uint8_t*>(buf_),n_);
        n_=0;
    }
private:
    void chr(char c) {
        if (n_<sizeof(buf_)) buf_[n_++]=c;
    }
    char buf_[108];
    uint8_t n_;
};

// Each report is a one-second window, not cumulative. RX/s counts frames
// successfully parsed by this ATmega, not merely hardware CRC acceptance.
struct Counters {
    uint32_t tx,reply,rx,queued,maxRt,empty,arc,bad,io,full;
    Counters() : tx(0),reply(0),rx(0),queued(0),maxRt(0),empty(0),
                 arc(0),bad(0),io(0),full(0) {}
    void reset() { *this=Counters(); }
};
static Counters measured;
static AckBenchPattern::Tracker txTracker,rxTracker;
static uint32_t lastReport=0;

static void report(uint8_t role) {
    const uint32_t now=clockTicks.now();
    if (static_cast<uint32_t>(now-lastReport)<2000000UL) return;
    lastReport=now;
    BenchLine l;
    if (role==0) {
        l.text("A TX/s=");l.dec(measured.tx);
        l.text(" ACK/s=");l.dec(measured.reply);
        l.text(" EMPTY=");l.dec(measured.empty);
        l.text(" MAX=");l.dec(measured.maxRt);
        l.text(" ARC=");l.dec(measured.arc);
        l.text(" BAD=");l.dec(measured.bad);
        l.text(" IO=");l.dec(measured.io);
        l.text(" GAP=");l.dec(txTracker.gaps());
    } else {
        l.text("B RX/s=");l.dec(measured.rx);
        l.text(" Q/s=");l.dec(measured.queued);
        l.text(" FULL=");l.dec(measured.full);
        l.text(" BAD=");l.dec(measured.bad);
        l.text(" IO=");l.dec(measured.io);
        l.text(" GAP=");l.dec(rxTracker.gaps());
    }
    l.send();
    measured.reset();
}

static void fail(const char* reason) {
    BenchLine l;
    l.text("ACK BENCH FAIL ");l.text(reason);l.send();
    for (;;) delay.ms(100);
}

static void runTx() {
    if (!phy.beginPtx()) fail("PTX");
    delay.ms(150); // initial peer startup only, not repeated per packet
    BenchLine hello;
    hello.text("ACK BENCH A PTX 32B/32B 1Mbps SPI8");
    hello.send();
    uint32_t seq=0;
    lastReport=clockTicks.now();
    for (;;) {
        AckBenchPattern::make(0,seq++,packet);
        const AckPhy::TxResult result=phy.transfer(packet,received);
        measured.arc+=result.retries;
        if (result.code==AckPhy::TxCode::ACK_WITH_DATA ||
            result.code==AckPhy::TxCode::ACK_EMPTY ||
            result.code==AckPhy::TxCode::BAD_REPLY_SIZE) {
            ++measured.tx;
            if (result.code==AckPhy::TxCode::ACK_WITH_DATA) {
                uint32_t answerSeq=0;
                if (AckBenchPattern::inspect(received,1,answerSeq)) {
                    ++measured.reply;
                    txTracker.observe(answerSeq);
                } else ++measured.bad;
            } else if (result.code==AckPhy::TxCode::ACK_EMPTY)
                ++measured.empty;
            else ++measured.bad;
        } else if (result.code==AckPhy::TxCode::MAX_RT)
            ++measured.maxRt;
        else ++measured.io;
        report(0);
    }
}

static void runRx() {
    if (!phy.beginPrx()) fail("PRX");
    AckBenchPattern::make(1,0,packet);
    if (phy.queueReply(packet)!=AckPhy::QueueCode::QUEUED)
        fail("PRIME_ACK");
    BenchLine hello;
    hello.text("ACK BENCH B PRX 32B/32B 1Mbps SPI8");
    hello.send();
    uint32_t nextReplySeq=1;
    lastReport=clockTicks.now();
    for (;;) {
        const AckPhy::RxCode status=phy.receive(received);
        if (status==AckPhy::RxCode::GOOD) {
            uint32_t remoteSeq=0;
            if (AckBenchPattern::inspect(received,0,remoteSeq)) {
                ++measured.rx;
                rxTracker.observe(remoteSeq);
                AckBenchPattern::make(1,nextReplySeq,packet);
                const AckPhy::QueueCode queued=phy.queueReply(packet);
                if (queued==AckPhy::QueueCode::QUEUED) {
                    ++nextReplySeq;
                    ++measured.queued;
                } else if (queued==AckPhy::QueueCode::FULL)
                    ++measured.full;
                else ++measured.io;
            } else ++measured.bad;
            if (!phy.clearPrxFlags()) ++measured.io;
        } else if (status==AckPhy::RxCode::BAD_SIZE) {
            ++measured.bad;
            if (!phy.clearPrxFlags()) ++measured.io;
        } else if (status==AckPhy::RxCode::IO_ERROR) {
            ++measured.io;
            if (!phy.clearPrxFlags()) ++measured.io;
        }
        report(1);
    }
}

int main() {
    uart.begin();
    Timer1Clock::begin();
    sei();
    radioCe.clear();
    radioCsn.set();
    delay.ms(110);
    const Radio::Probe probe=radio.probeAndConfigure();
    if (probe.result!=Radio::Result::OK) fail("NRF_PROBE");
    // Faster SPI is optional in the driver and affects ONLY this app.
    spi.setSpeed(SpiMaster::Speed::MHz8);
    if (!radio.enableAckPayload(0x23)) fail("FEATURE_OR_DPL");

    uint16_t adc=0;
    if (!Adc0::sample(adc)) fail("ADC0");
    if (Adc0::classForM1(adc)==0) runTx();
    else runRx();
}

#endif // GPT_NRF24_APPLICATION == 1
