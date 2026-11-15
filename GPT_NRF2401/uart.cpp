#include "uart.h"
#include "board_clock.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

#define UART_BAUD_DIVISOR 16U
#define UART_RX_ERROR_MASK (_BV(FE0) | _BV(DOR0) | _BV(UPE0))

RetainedByteRing uart_rx_buffer;
volatile uint8_t uart_tx_buffer[UART_TX_BUFFER_SIZE];
volatile uint8_t uart_tx_write_position = 0;
volatile uint8_t uart_tx_read_position = 0;

// These counters change only on an error path. Normal RX needs no counter.
static volatile uint16_t uart_rx_full_count = 0;
static volatile uint16_t uart_rx_framing_count = 0;
static volatile uint16_t uart_rx_overrun_count = 0;
static volatile uint16_t uart_rx_parity_count = 0;
static uint16_t uart_tx_full_count = 0;
#if PP_UART_DIAG_ENABLED
static volatile uint16_t uart_rx_byte_count = 0;
static volatile uint16_t uart_tx_byte_count = 0;
#endif

UART::UART() {
}

void UART::begin() {
	UCSR0B = 0;
	uart_rx_buffer.reset();
	uart_tx_write_position = 0;
	uart_tx_read_position = 0;
	uart_rx_full_count = 0;
	uart_rx_framing_count = 0;
	uart_rx_overrun_count = 0;
	uart_rx_parity_count = 0;
	uart_tx_full_count = 0;
#if PP_UART_DIAG_ENABLED
	uart_rx_byte_count = 0;
	uart_tx_byte_count = 0;
#endif
	UCSR0A = _BV(U2X0);
	UBRR0H = 0;
	UBRR0L = UART_BAUD_DIVISOR;
	UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
	UCSR0B = _BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0);
}

void UART::end() {
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		UCSR0B = 0;
	}
}

bool UART::read(uint8_t& value) {
	return uart_rx_buffer.pop(value);
}

uint8_t UART::available() const {
	return uart_rx_buffer.available();
}

uint8_t UART::freeSpace() const {
	const uint8_t used = (uart_tx_write_position - uart_tx_read_position) & UART_TX_BUFFER_MASK;
	return UART_TX_BUFFER_MASK - used;
}

bool UART::write(uint8_t value) {
	return writeAll(&value, 1);
}

uint8_t UART::write(const uint8_t* bytes, uint8_t count) {
	if (!bytes) {
		return 0;
	}
	uint8_t written = 0;
	while (written < count && write(bytes[written])) {
		++written;
	}
	return written;
}

// One producer in foreground. ISR only frees TX slots during this copy.
// Publish the complete segment once, before enabling the UDRE interrupt.
bool UART::writeAll(const uint8_t* bytes, uint8_t count) {
	if (count != 0 && !bytes) {
		return false;
	}
	if (freeSpace() < count) {
		++uart_tx_full_count;
		return false;
	}
	uint8_t position = uart_tx_write_position;
	for (uint8_t i = 0; i < count; ++i) {
		uart_tx_buffer[position] = bytes[i];
		position = (position + 1U) & UART_TX_BUFFER_MASK;
	}
	if (count != 0) {
		uart_tx_write_position = position;
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
			UCSR0B |= _BV(UDRIE0);
		}
	}
	return true;
}

UART::Counters UART::counters() const {
	Counters result;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		result.rx_buffer_full = uart_rx_full_count;
		result.rx_framing = uart_rx_framing_count;
		result.rx_overrun = uart_rx_overrun_count;
		result.rx_parity = uart_rx_parity_count;
		result.tx_buffer_full = uart_tx_full_count;
#if PP_UART_DIAG_ENABLED
		result.rx_bytes = uart_rx_byte_count;
		result.tx_bytes = uart_tx_byte_count;
#endif
	}
	return result;
}

// No method dispatch, function call, pause calculation or protocol work.
// Sampling status before UDR is required by the AVR USART hardware.
ISR(USART_RX_vect) {
	const uint8_t status = UCSR0A;
	const uint8_t value = UDR0;
	if ((status & UART_RX_ERROR_MASK) != 0) {
		if ((status & _BV(FE0)) != 0) {
			++uart_rx_framing_count;
		}
		if ((status & _BV(DOR0)) != 0) {
			++uart_rx_overrun_count;
		}
		if ((status & _BV(UPE0)) != 0) {
			++uart_rx_parity_count;
		}
		return;
	}
	const uint8_t position = uart_rx_buffer.write_position;
	const uint8_t next = position + 1U;
	if (next == uart_rx_buffer.acknowledged_position) {
		++uart_rx_full_count;
		return;
	}
	uart_rx_buffer.bytes[position] = value;
	uart_rx_buffer.write_position = next;
#if PP_UART_DIAG_ENABLED
	++uart_rx_byte_count;
#endif
}

ISR(USART_UDRE_vect) {
	const uint8_t position = uart_tx_read_position;
	if (position == uart_tx_write_position) {
		UCSR0B &= ~_BV(UDRIE0);
		return;
	}
	UDR0 = uart_tx_buffer[position];
	uart_tx_read_position = (position + 1U) & UART_TX_BUFFER_MASK;
#if PP_UART_DIAG_ENABLED
	++uart_tx_byte_count;
#endif
}

RetainedByteRing& UART::retainedInput() {
	return uart_rx_buffer;
}

bool UART::rxPauseRequested() const {
	return uart_rx_buffer.pauseRequested();
}
