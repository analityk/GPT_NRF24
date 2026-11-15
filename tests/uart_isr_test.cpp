#include "../GPT_NRF2401/uart.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <cassert>

#define UART_ISR_TEST_WRAP_ROUNDS 4U
#define UART_ISR_TEST_SEGMENT_BYTES 24U
#define UART_ISR_TEST_DROPPED_BYTE 0xA5U
#define UART_ISR_TEST_ERROR_FLAGS (_BV(FE0) | _BV(DOR0) | _BV(UPE0))

volatile uint8_t UCSR0A = 0;
volatile uint8_t UCSR0B = 0;
volatile uint8_t UCSR0C = 0;
volatile uint8_t UBRR0H = 0;
volatile uint8_t UBRR0L = 0;
volatile uint8_t UDR0 = 0;

static void receive(uint8_t value, uint8_t status = 0) {
	UCSR0A = status;
	UDR0 = value;
	USART_RX_vect();
}

static void retainedRxAndWrap() {
	UART uart;
	uart.begin();
	for (unsigned round = 0; round < UART_ISR_TEST_WRAP_ROUNDS; ++round) {
		const uint8_t start = uart_rx_buffer.write_position;
		for (unsigned i = 0; i < RETAINED_RING_MAX_BYTES; ++i) {
			receive(i);
		}
		assert(uart.available() == RETAINED_RING_MAX_BYTES);
		assert(uart.rxPauseRequested());
		uart_rx_buffer.markPrepared(RETAINED_RING_MAX_BYTES);
		// Preparing radio DATA does not release storage. A full ISR must leave
		// the accepted prefix and both consumer cursors intact for retransmission.
		const uint8_t stopped = uart_rx_buffer.write_position;
		receive(UART_ISR_TEST_DROPPED_BYTE);
		assert(uart_rx_buffer.write_position == stopped);
		assert(uart.counters().rx_buffer_full == round + 1U);
		for (unsigned i = 0; i < RETAINED_RING_MAX_BYTES; ++i) {
			const uint8_t index = start + i;
			assert(uart_rx_buffer.at(index) == uint8_t(i));
		}
		uart_rx_buffer.acknowledge(RETAINED_RING_MAX_BYTES);
		assert(uart.available() == 0);
		assert(!uart.rxPauseRequested());
	}
	const uint8_t before = uart_rx_buffer.write_position;
	receive(UART_ISR_TEST_DROPPED_BYTE, UART_ISR_TEST_ERROR_FLAGS);
	assert(uart_rx_buffer.write_position == before);
	const UART::Counters counts = uart.counters();
	assert(counts.rx_framing == 1);
	assert(counts.rx_overrun == 1);
	assert(counts.rx_parity == 1);
}

static void txCommitAndEmptyInterrupt() {
	UART uart;
	uart.begin();
	uint8_t segment[UART_ISR_TEST_SEGMENT_BYTES];
	for (unsigned i = 0; i < sizeof(segment); ++i) {
		segment[i] = i;
	}
	for (unsigned round = 0; round < UART_ISR_TEST_WRAP_ROUNDS; ++round) {
		while (uart.freeSpace() >= sizeof(segment)) {
			assert(uart.writeAll(segment, sizeof(segment)));
		}
		const uint8_t before = uart_tx_write_position;
		assert(!uart.writeAll(segment, sizeof(segment)));
		assert(uart_tx_write_position == before);
		unsigned sent = 0;
		while (uart_tx_read_position != uart_tx_write_position) {
			USART_UDRE_vect();
			assert(UDR0 == segment[sent % sizeof(segment)]);
			++sent;
		}
		assert(sent != 0);
		USART_UDRE_vect();
		assert((UCSR0B & _BV(UDRIE0)) == 0);
		assert(uart.freeSpace() == UART_TX_BUFFER_MASK);
	}
	assert(!uart.writeAll(0, 1));
	assert(uart.writeAll(0, 0));
	uart.end();
	assert(UCSR0B == 0);
}

int main() {
	retainedRxAndWrap();
	txCommitAndEmptyInterrupt();
}
