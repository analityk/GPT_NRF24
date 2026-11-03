#include "uart.h"
#include "board_clock.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

// Board clock assumption. 16 MHz, U2X0=1, UBRR0=16:
// baud = 16e6 / (8 * (16 + 1)) = 117647, +2.12% vs 115200.
UART* UART::active_ = 0;

UART::UART()
    : rx_buffer_full_(0), rx_framing_(0), rx_overrun_(0),
      rx_parity_(0), tx_buffer_full_(0) {}

void UART::begin() {
    // Exclusive USART0 owner; call before sei(). Reinitialization drops queued bytes.
    UCSR0B = 0;
    active_ = this;
    rx_.reset();
    tx_.reset();
    rx_buffer_full_ = rx_framing_ = rx_overrun_ = rx_parity_ = tx_buffer_full_ = 0;
    UCSR0A = _BV(U2X0);
    UBRR0H = 0;
    UBRR0L = 16;
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00); // async, no parity, 1 stop, 8 bits
    UCSR0B = _BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0);
}

void UART::end() {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        UCSR0B = 0;
        if (active_ == this) active_ = 0;
    }
}

bool UART::read(uint8_t& value) { return rx_.pop(value); }
uint8_t UART::available() const { return rx_.available(); }
uint8_t UART::freeSpace() const { return tx_.freeSpace(); }

bool UART::write(uint8_t value) {
    bool ok;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        ok = tx_.push(value);
        if (ok) UCSR0B |= _BV(UDRIE0);
        else ++tx_buffer_full_;
    }
    return ok;
}

uint8_t UART::write(const uint8_t* bytes, uint8_t count) {
    if (!bytes) return 0;
    uint8_t n = 0;
    while (n < count && write(bytes[n])) ++n;
    return n;
}

// A transport segment must not be partially accepted. TX has a single
// foreground producer; USART_UDRE_vect is its only consumer. Masking
// interrupts here preserves the all-or-none contract even if other code
// is added later. Maximum transport segment is 18 bytes.
bool UART::writeAll(const uint8_t* bytes, uint8_t count) {
    if (count!=0 && !bytes) return false;
    bool ok=false;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        if (tx_.freeSpace() >= count) {
            for (uint8_t i=0; i<count; ++i)
                (void)tx_.push(bytes[i]);
            if (count) UCSR0B |= _BV(UDRIE0);
            ok=true;
        } else {
            ++tx_buffer_full_;
        }
    }
    return ok;
}

UART::Counters UART::counters() const {
    Counters result;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        result.rx_buffer_full = rx_buffer_full_;
        result.rx_framing = rx_framing_;
        result.rx_overrun = rx_overrun_;
        result.rx_parity = rx_parity_;
        result.tx_buffer_full = tx_buffer_full_;
    }
    return result;
}

void UART::receiveInterrupt() {
    // Status bits must be sampled before reading UDR0.
    const uint8_t status = UCSR0A;
    const uint8_t value = UDR0;
    UART* uart = active_;
    if (!uart) return;

    bool error = false;
    if (status & _BV(FE0)) { ++uart->rx_framing_; error = true; }
    if (status & _BV(DOR0)) { ++uart->rx_overrun_; error = true; }
    if (status & _BV(UPE0)) { ++uart->rx_parity_; error = true; }
    if (!error && !uart->rx_.push(value)) ++uart->rx_buffer_full_;
}

void UART::transmitInterrupt() {
    UART* uart = active_;
    if (!uart) {
        UCSR0B &= static_cast<uint8_t>(~_BV(UDRIE0));
        return;
    }
    uint8_t value;
    if (uart->tx_.pop(value)) UDR0 = value;
    else UCSR0B &= static_cast<uint8_t>(~_BV(UDRIE0));
}

ISR(USART_RX_vect) { UART::receiveInterrupt(); }
ISR(USART_UDRE_vect) { UART::transmitInterrupt(); }
