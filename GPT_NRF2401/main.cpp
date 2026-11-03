/*
 * SPI/nRF24L01+ bring-up over the previously verified UART0.
 * The radio stays powered down (no RF TX); '?' repeats diagnostics.
 * Other bytes still echo locally after the initial report.
 *
 * Legacy ATmega328P RF-Nano wiring (manufacturer's v1/v2):
 * D9 / PB1 = nRF CSN, D10 / PB2 = nRF CE.
 * For other RF-Nano revisions change only these board pin bindings.
 */
#ifndef F_CPU
#define F_CPU 16000000UL
#endif
#if F_CPU != 16000000UL
#error "RF-Nano bring-up assumes an ATmega328P at 16 MHz"
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "gpio.h"
#include "uart.h"
#include "spi.h"
#include "nrf24.h"

static UART uart;
static SpiMaster spi;
static GPIO radioCe(&PORTB, PB2);   // board wiring D10: CE (also AVR hardware SS)
static GPIO radioCsn(&PORTB, PB1);  // board wiring D9: CSN
static Nrf24<SpiMaster, GPIO> radio(spi, radioCe, radioCsn);

class DiagnosticLine {
public:
    DiagnosticLine() : length_(0), cursor_(0) {}
    void set(const Nrf24<SpiMaster, GPIO>::Probe& p) {
        length_=0;
        cursor_=0;
        append("NRF24 ");
        if (p.result == Nrf24<SpiMaster, GPIO>::Result::OK) {
            append("OK S="); hex(p.status);
            append(" CFG="); hex(p.config);
            append(" CH="); hex(p.channel);
            append(" RF="); hex(p.rfSetup);
            append(" FIFO="); hex(p.fifoStatus);
        } else {
            append("FAIL C="); hex(static_cast<uint8_t>(p.result));
            append(" REG="); hex(p.reg);
            append(" EXP="); hex(p.expected);
            append(" GOT="); hex(p.observed);
            append(" S="); hex(p.status);
            append(" CH="); hex(p.channel);
        }
        append("\r\n");
    }
    bool pending() const { return cursor_ != length_; }
    void service(UART& port) {
        if (pending() && port.freeSpace() != 0 &&
            port.write(static_cast<uint8_t>(data_[cursor_])))
            ++cursor_;
    }
private:
    void character(char c) {
        if (length_ < sizeof(data_)) data_[length_++]=c;
    }
    void append(const char* s) {
        while (*s) character(*s++);
    }
    void hex(uint8_t value) {
        digit(static_cast<uint8_t>(value >> 4));
        digit(static_cast<uint8_t>(value & 15U));
    }
    void digit(uint8_t v) {
        character(static_cast<char>(v < 10U ? ('0' + v) : ('A' + v - 10U)));
    }
    char data_[96];
    uint8_t length_;
    uint8_t cursor_;
};

static DiagnosticLine diagnostic;

int main(void) {
    uart.begin();
    sei();

    // nRF24L01+ POR may require up to 100 ms after the supply reaches 1.9 V.
    // Put the RF control pins into idle states *before* the delay.
    radioCe.clear();
    radioCsn.set();
    _delay_ms(110);

    diagnostic.set(radio.probeAndConfigure());

    for (;;) {
        // UART IRQs continue running while SPI is polled in the foreground.
        diagnostic.service(uart);

        if (!diagnostic.pending() && uart.freeSpace() != 0) {
            uint8_t byte;
            if (uart.read(byte)) {
                if (byte == '?') {
                    diagnostic.set(radio.probeAndConfigure());
                } else {
                    (void)uart.write(byte);
                }
            }
        }
    }
}
