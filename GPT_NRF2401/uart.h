#ifndef GPT_NRF24_UART_H
#define GPT_NRF24_UART_H

#include <stdint.h>
#include "byte_ring.h"
#include "retained_byte_ring.h"
#include "pp_test_traffic_mode.h"

// USART0 hardware driver for ATmega328P, 16 MHz, 115200 8N1.
// Exactly one UART instance may own USART0 at a time.
// begin() configures hardware; caller explicitly enables global interrupts (sei()).
class UART {
public:
    struct Counters {
        uint16_t rx_buffer_full;
        uint16_t rx_framing;
        uint16_t rx_overrun;
        uint16_t rx_parity;
        uint16_t tx_buffer_full;
#if PP_UART_DIAG_ENABLED
		uint16_t rx_bytes;
		uint16_t tx_bytes;
#endif
    };

    UART();
    void begin();
    void end();
    bool read(uint8_t& value);          // nonblocking
    bool write(uint8_t value);         // nonblocking; false => NOT queued
    uint8_t write(const uint8_t* bytes, uint8_t count); // prefix actually queued
    bool writeAll(const uint8_t* bytes, uint8_t count); // atomic all-or-none TX enqueue
    uint8_t available() const;
	RetainedByteRing& retainedInput(){
		return rx_;
	}
	bool rxPauseRequested() const{
		return rx_.pauseRequested();
	}
    uint8_t freeSpace() const;         // TX queue free capacity
    Counters counters() const;         // atomic snapshot

    // Interrupt entry points, used only by ISR wrappers.
    static void receiveInterrupt();
    static void transmitInterrupt();

private:
    UART(const UART&) = delete;
    UART& operator=(const UART&) = delete;

    static UART* active_;
    RetainedByteRing rx_;
    ByteRing<128> tx_;
    volatile uint16_t rx_buffer_full_;
    volatile uint16_t rx_framing_;
    volatile uint16_t rx_overrun_;
    volatile uint16_t rx_parity_;
    volatile uint16_t tx_buffer_full_;
#if PP_UART_DIAG_ENABLED
	volatile uint16_t rx_bytes_;
	volatile uint16_t tx_bytes_;
#endif
};
#endif
