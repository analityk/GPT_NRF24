#ifndef GPT_NRF24_UART_H
#define GPT_NRF24_UART_H

#include <stdint.h>
#include "foreground_ports.h"
#include "byte_ring.h"
#include "retained_byte_ring.h"
#include "pp_test_traffic_mode.h"

// USART0 hardware driver for ATmega328P, 16 MHz, 115200 8N1.
// USART0 is one global peripheral; UART is its foreground facade.
// begin() configures hardware; caller explicitly enables global interrupts (sei()).
#define UART_TX_BUFFER_SIZE 128U
#define UART_TX_BUFFER_MASK (UART_TX_BUFFER_SIZE - 1U)
extern RetainedByteRing uart_rx_buffer;
extern volatile uint8_t uart_tx_buffer[UART_TX_BUFFER_SIZE];
extern volatile uint8_t uart_tx_write_position;
extern volatile uint8_t uart_tx_read_position;

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
	bool read(uint8_t& value);							// nonblocking
	bool write(uint8_t value);							// nonblocking; false => NOT queued
	uint8_t write(const uint8_t* bytes, uint8_t count); // prefix actually queued
	bool writeAll(const uint8_t* bytes, uint8_t count); // atomic all-or-none TX enqueue
	uint8_t available() const;
	RetainedByteRing& retainedInput();
	bool rxPauseRequested() const;
	uint8_t freeSpace() const; // TX queue free capacity
	Counters counters() const; // atomic snapshot

  private:
	UART(const UART&) = delete;
	UART& operator=(const UART&) = delete;
};
#endif
