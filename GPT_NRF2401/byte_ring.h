#ifndef GPT_NRF24_BYTE_RING_H
#define GPT_NRF24_BYTE_RING_H

#include <stdint.h>
#define BYTE_RING_CAPACITY 128U
#define BYTE_RING_MIN_CAPACITY 2U
#define BYTE_RING_MAX_CAPACITY (UINT8_MAX + 1U)
static_assert(BYTE_RING_CAPACITY >= BYTE_RING_MIN_CAPACITY &&
				  BYTE_RING_CAPACITY <= BYTE_RING_MAX_CAPACITY &&
				  (BYTE_RING_CAPACITY & (BYTE_RING_CAPACITY - 1U)) == 0,
	"Byte ring must fit an atomic byte cursor and have power-of-two capacity");

// Single-producer / single-consumer ring. BYTE_RING_CAPACITY must be a power of two.
// One slot stays empty to distinguish full from empty. No heap allocation.
// Head/tail and byte accesses are 8-bit and atomic on ATmega328P.
// Ownership: only one context pushes and only one context pops.
class ByteRing {

  public:
	ByteRing();

	bool push(uint8_t value);

	bool pop(uint8_t& value);

	uint8_t available() const;

	uint8_t freeSpace() const;

	void reset(); // Only when producer/consumer are stopped.
  private:
	volatile uint8_t data_[BYTE_RING_CAPACITY];
	volatile uint8_t head_;
	volatile uint8_t tail_;
};
#endif
