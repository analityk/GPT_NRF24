/*
 * RF-Nano raw 32-byte bidirectional radio hardware test.
 *
 * Same firmware on both boards. ADC0/GND => role 0 (initiator);
 * ADC0/3.3V => role 1 (responder), assuming AVCC of 3.3V or 5V.
 * No protocol-v1 headers, no UART-to-radio bridge, no routing.
 *
 * Board wiring verified during SPI test: PB2/D10 = CE, PB1/D9 = CSN.
 * nRF24 SPI command layer is reused unchanged.
 */
#ifndef F_CPU
#define F_CPU 16000000UL
#endif
#if F_CPU != 16000000UL
#error "16 MHz clock required for RF hardware test"
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include "gpio.h"
#include "uart.h"
#include "spi.h"
#include "nrf24.h"
#include "nrf24_phy.h"
#include "avr_delay.h"
#include "adc0.h"
#include "rf_raw_pattern.h"

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
static uint8_t payload[32]; // single reusable buffer
static uint16_t goodRx=0;
static uint16_t invalidRx=0;
static uint16_t txAck=0;
static uint16_t txFail=0;
static uint16_t replyTimeout=0;

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
    logLine.send();
}

static void commands() {
    uint8_t input=0;
    while (uart.read(input)) {
        if (input=='?') reportCounters();
    }
}

static bool pollRx(bool& received) {
    if (!phy.receive(payload,received)) {
        logLine.reset();
        logLine.text("RADIO SPI ERROR RX");
        logLine.send();
        return false;
    }
    return true;
}

static void initiatorLoop() {
    delay.ms(200); // allow later-powered peer to enter PRX; recovery is periodic
    uint16_t nextSeq=0;
    for (;;) {
        commands();
        RawRfPattern::make(payload,nextSeq);
        const auto tx=phy.transmit(payload);
        if (tx.result==decltype(tx.result)::ACKED) ++txAck;
        else ++txFail;

        bool gotReply=false;
        if (!phy.listen()) {
            logLine.reset(); logLine.text("RADIO SPI ERROR ENTER_RX"); logLine.send();
        } else {
            for (uint8_t ms=0; ms<100; ++ms) {
                bool received=false;
                if (!pollRx(received)) break;
                if (received) {
                    uint16_t actual=0;
                    if (RawRfPattern::validate(payload,RawRfPattern::REPLY,actual) && actual==nextSeq) {
                        ++goodRx;
                        gotReply=true;
                        break;
                    }
                    ++invalidRx;
                }
                delay.ms(1);
            }
        }
        phy.standby();
        if (!gotReply) ++replyTimeout;
        logLine.reset();
        logLine.text("A SEQ="); logLine.hex16(nextSeq);
        logLine.text(" TX="); logLine.text(txName(tx.result));
        logLine.text(" ARC="); logLine.hex8(tx.arc);
        logLine.text(" REPLY="); logLine.text(gotReply ? "OK" : "MISS");
        logLine.send();
        ++nextSeq;
        delay.ms(200); // responder has time to return to PRX
    }
}

static void responderLoop() {
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
        uint16_t seq=0;
        if (!RawRfPattern::validate(payload,RawRfPattern::REQUEST,seq)) {
            ++invalidRx;
            logLine.reset(); logLine.text("B BAD_FRAME"); logLine.send();
            continue;
        }
        ++goodRx;
        // RX_DR may be asserted before hardware ACK completes. The peer can
        // still be retrying if an ACK was lost. Preserve a generous guard.
        delay.ms(25);
        RawRfPattern::markReply(payload); // return the same 31 bytes
        const auto tx=phy.transmit(payload);
        if (tx.result==decltype(tx.result)::ACKED) ++txAck;
        else ++txFail;
        if (!phy.listen()) {
            logLine.reset(); logLine.text("RADIO SPI ERROR ENTER_RX"); logLine.send();
        }
        logLine.reset();
        logLine.text("B SEQ="); logLine.hex16(seq);
        logLine.text(" RX=OK TX="); logLine.text(txName(tx.result));
        logLine.text(" ARC="); logLine.hex8(tx.arc);
        logLine.send();
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
    logLine.text(" MODE=RAW32");
    logLine.send();

    if (nodeClass==0) initiatorLoop();
    else responderLoop();
}
