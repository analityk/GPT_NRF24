/*
 * V1 payload bring-up: fixed 32 B over working bidirectional nRF24 PHY.
 * Same binary on both boards: ADC0 to GND = class 0 (initiator),
 * ADC0 to AVCC or 3.3V = class 1 (responder).
 * Sends length 0/1/23/24 byte test frames. NO UART bridge yet.
 * TEST_SID is a fixture, NOT session negotiation or reliable transport.
 * RF pins: PB2/D10 CE, PB1/D9 CSN (verified on these RF-Nanos).
 */
#include "board_clock.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include "gpio.h"
#include "uart.h"
#include "spi.h"
#include "nrf24.h"
#include "nrf24_phy.h"
#include "avr_delay.h"
#include "adc0.h"
#include "frame_v1.h"
#include "v1_probe_pattern.h"

static UART uart;
static SpiMaster spi;
static GPIO radioCe(&PORTB, PB2);
static GPIO radioCsn(&PORTB, PB1);
static Nrf24<SpiMaster, GPIO> radio(spi, radioCe, radioCsn);
static AvrDelay delay;
static Nrf24Phy<Nrf24<SpiMaster, GPIO>, AvrDelay> phy(radio, delay);

class LogLine {
public:
    LogLine() : n_(0) {}
    void reset() { n_=0; }
    void text(const char* s) { while (*s) character(*s++); }
    void hex8(uint8_t v) {
        digit(static_cast<uint8_t>(v >> 4));
        digit(static_cast<uint8_t>(v & 0x0FU));
    }
    void hex16(uint16_t v) {
        hex8(static_cast<uint8_t>(v >> 8));
        hex8(static_cast<uint8_t>(v));
    }
    void send() {
        text("\r\n");
        // All-or-nothing: diagnostics never partially fill the UART stream.
        if (uart.freeSpace() >= n_) {
            (void)uart.write(reinterpret_cast<const uint8_t*>(buf_), n_);
        }
        n_=0;
    }
private:
    void digit(uint8_t d) {
        character(static_cast<char>(d < 10U ? '0'+d : 'A'+d-10U));
    }
    void character(char c) {
        if (n_<sizeof(buf_)) buf_[n_++]=c;
    }
    char buf_[80];
    uint8_t n_;
};

static LogLine logLine;
static uint8_t payload[FrameV1::FRAME_SIZE]; // single reusable RF buffer
static uint16_t goodRx=0;
static uint16_t invalidRx=0;
static uint16_t txAck=0;
static uint16_t txFail=0;
static uint16_t replyTimeout=0;
static uint16_t rxDrSeen=0;
static uint16_t rejectedFrames=0;
static bool lastRxDr=false;

static const char* txName(Nrf24Phy<Nrf24<SpiMaster, GPIO>, AvrDelay>::TxStatus s) {
    typedef Nrf24Phy<Nrf24<SpiMaster, GPIO>, AvrDelay> Phy;
    switch (s) {
        case Phy::TxStatus::ACKED: return "ACK";
        case Phy::TxStatus::MAX_RETRIES: return "MAX_RT";
        case Phy::TxStatus::TIMEOUT: return "TIMEOUT";
        default: return "SPI_ERR";
    }
}

static void reportCounters() {
    logLine.reset();
    logLine.text("COUNT RX="); logLine.hex16(goodRx);
    logLine.text(" BAD="); logLine.hex16(invalidRx);
    logLine.text(" TX_ACK="); logLine.hex16(txAck);
    logLine.text(" TX_FAIL="); logLine.hex16(txFail);
    logLine.text(" REPLY_TO="); logLine.hex16(replyTimeout);
    logLine.text(" RX_DR="); logLine.hex16(rxDrSeen);
    logLine.text(" REJECT="); logLine.hex16(rejectedFrames);
    logLine.send();
}

static void commands() {
    uint8_t input=0;
    while (uart.read(input)) {
        if (input=='?') reportCounters();
    }
}

static bool pollRx(bool& received) {
    bool sawRxDr=false;
    if (!phy.receive(payload,received,&sawRxDr)) {
        logLine.reset();
        logLine.text("RADIO SPI ERROR RX");
        logLine.send();
        return false;
    }
    if (received) {
        lastRxDr=sawRxDr;
        if (sawRxDr) ++rxDrSeen;
    }
    return true;
}

static void initiatorLoop() {
    delay.ms(200); // responder may power up at a different time
    uint8_t nextSeq=0;
    uint8_t lastReplySeq=0xFF;
    for (;;) {
        commands();
        const uint8_t wantedLength=V1ProbePattern::length(nextSeq,0);
        if (V1ProbePattern::make(0,1,nextSeq,lastReplySeq,payload)
                != FrameV1::Error::OK) {
            logLine.reset(); logLine.text("V1 ENCODE ERROR A"); logLine.send();
            delay.ms(100);
            continue;
        }
        const auto tx=phy.transmit(payload);
        if (tx.result==decltype(tx.result)::ACKED) ++txAck;
        else ++txFail;

        bool gotReply=false;
        bool replyRxDr=false;
        uint8_t receivedLength=0;
        if (!phy.listen()) {
            logLine.reset(); logLine.text("RADIO SPI ERROR ENTER_RX"); logLine.send();
        } else {
            for (uint8_t ms=0; ms<100; ++ms) {
                bool received=false;
                if (!pollRx(received)) break;
                if (received) {
                    FrameV1::View view;
                    const FrameV1::Error parsed=FrameV1::decode(payload,view);
                    if (parsed==FrameV1::Error::OK &&
                        V1ProbePattern::inspect(view,1,0) &&
                        view.header.rxLast==nextSeq) {
                        lastReplySeq=view.header.txSeq;
                        receivedLength=view.header.length;
                        replyRxDr=lastRxDr;
                        ++goodRx;
                        gotReply=true;
                        break;
                    }
                    ++invalidRx;
                    if (parsed!=FrameV1::Error::OK) ++rejectedFrames;
                }
                delay.ms(1);
            }
        }
        phy.standby();
        if (!gotReply) ++replyTimeout;
        logLine.reset();
        logLine.text("A SEQ="); logLine.hex8(nextSeq);
        logLine.text(" LEN="); logLine.hex8(wantedLength);
        logLine.text(" TX="); logLine.text(txName(tx.result));
        logLine.text(" ARC="); logLine.hex8(tx.arc);
        logLine.text(" REPLY="); logLine.text(gotReply ? "OK" : "MISS");
        logLine.text(" IN="); logLine.hex8(receivedLength);
        logLine.text(" RD="); logLine.text((gotReply && replyRxDr) ? "1" : "0");
        logLine.send();
        ++nextSeq; // v1 TX_SEQ wraps modulo 256
        delay.ms(200); // diagnostic only, not the M1 slot scheduler
    }
}

static void responderLoop() {
    uint8_t nextSeq=0; // own TX_SEQ is independent of initiator's
    if (!phy.listen()) {
        logLine.reset(); logLine.text("RADIO SPI ERROR ENTER_RX"); logLine.send();
    }
    for (;;) {
        commands();
        bool received=false;
        if (!pollRx(received)) {
            delay.ms(100);
            (void)phy.listen();
            continue;
        }
        if (!received) {
            delay.ms(1);
            continue;
        }
        FrameV1::View view;
        const FrameV1::Error parsed=FrameV1::decode(payload,view);
        if (parsed!=FrameV1::Error::OK ||
            !V1ProbePattern::inspect(view,0,1)) {
            ++invalidRx;
            if (parsed!=FrameV1::Error::OK) ++rejectedFrames;
            logLine.reset();
            logLine.text("B BAD_FRAME E=");
            logLine.hex8(static_cast<uint8_t>(parsed));
            logLine.send();
            continue;
        }
        ++goodRx;
        const uint8_t receivedSeq=view.header.txSeq;
        const uint8_t receivedLength=view.header.length;
        const bool observedRxDr=lastRxDr;
        // nRF may assert RX_DR before hardware ACK exchange finishes.
        delay.ms(25);
        const uint8_t responseLength=V1ProbePattern::length(nextSeq,1);
        if (V1ProbePattern::make(1,0,nextSeq,receivedSeq,payload)
                != FrameV1::Error::OK) {
            logLine.reset(); logLine.text("V1 ENCODE ERROR B"); logLine.send();
            (void)phy.listen();
            continue;
        }
        const auto tx=phy.transmit(payload);
        if (tx.result==decltype(tx.result)::ACKED) ++txAck;
        else ++txFail;
        if (!phy.listen()) {
            logLine.reset(); logLine.text("RADIO SPI ERROR ENTER_RX"); logLine.send();
        }
        logLine.reset();
        logLine.text("B SEQ="); logLine.hex8(receivedSeq);
        logLine.text(" LEN="); logLine.hex8(receivedLength);
        logLine.text(" RX=OK TX="); logLine.text(txName(tx.result));
        logLine.text(" ARC="); logLine.hex8(tx.arc);
        logLine.text(" OUT="); logLine.hex8(responseLength);
        logLine.text(" RD="); logLine.text(observedRxDr ? "1" : "0");
        logLine.send();
        ++nextSeq;
    }
}

int main() {
    uart.begin();
    sei();

    // Power on reset time: nRF needs a settled supply before SPI setup.
    radioCe.clear();
    radioCsn.set();
    delay.ms(110);
    const auto probe=radio.probeAndConfigure();
    if (probe.result!=decltype(probe.result)::OK) {
        logLine.reset();
        logLine.text("NRF FAIL C=");
        logLine.hex8(static_cast<uint8_t>(probe.result));
        logLine.text(" REG="); logLine.hex8(probe.reg);
        logLine.send();
        for (;;) { commands(); delay.ms(100); }
    }
    uint16_t rawAdc=0;
    if (!Adc0::sample(rawAdc)) {
        logLine.reset(); logLine.text("ADC0 FAILED"); logLine.send();
        for (;;) { commands(); delay.ms(100); }
    }
    const uint8_t nodeClass=Adc0::classForM1(rawAdc);
    logLine.reset();
    logLine.text("NRF OK CLASS="); logLine.hex8(nodeClass);
    logLine.text(" ADC="); logLine.hex16(rawAdc);
    logLine.text(" MODE=V1PAYLOAD");
    logLine.send();

    if (nodeClass==0) initiatorLoop();
    else responderLoop();
}
