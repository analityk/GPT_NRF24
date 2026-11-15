#ifndef GPT_NRF24_RETAINED_BYTE_RING_H
#define GPT_NRF24_RETAINED_BYTE_RING_H

#include <stdint.h>

// A 256-byte SPSC ring for ACK-retained transmission. One byte is kept
// unused to distinguish full/empty. Every cursor fits one atomic AVR byte.
// ISR owns write_position; foreground owns prepared_position and acknowledged_position.
// Storage is never cleared when an ACK advances acknowledged_position.
#define RETAINED_RING_CAPACITY 256U
static_assert(
	RETAINED_RING_CAPACITY == UINT8_MAX + 1U, "Retained RX cursors wrap at exactly 256 bytes");
#define RETAINED_RING_MASK 0xFFU
#define RETAINED_RING_MAX_BYTES 255U
#define RETAINED_RING_PAUSE_FREE_BYTES 48U
#define RETAINED_RING_RESUME_FREE_BYTES 96U

class RetainedByteRing {
  public:
	RetainedByteRing();

	bool push(uint8_t byte);

	// Ordinary consuming reader, for uses that do not retain UART data
	// (for example benchmark control commands).
	bool pop(uint8_t& byte);

	uint8_t available() const;

	uint8_t availableToPrepare() const;

	uint8_t freeSpace() const;

	uint8_t prepared() const;

	uint8_t at(uint8_t index) const;

	// The caller has already serialized these bytes into a complete frame.
	void markPrepared(uint8_t count);

	// Only a cumulative application ACK may release retained bytes.
	void acknowledge(uint8_t count);

	bool pauseRequested() const;

	void reset();

  public:
	// RX ISR publishes write_position; foreground alone releases acknowledged_position.
	volatile uint8_t bytes[RETAINED_RING_CAPACITY];
	volatile uint8_t write_position;
	uint8_t prepared_position;
	volatile uint8_t acknowledged_position;
	mutable bool pause_;
};

#endif
