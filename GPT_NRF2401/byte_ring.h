#ifndef GPT_NRF24_BYTE_RING_H
#define GPT_NRF24_BYTE_RING_H

#include <stdint.h>

// Single-producer / single-consumer ring. Capacity must be a power of two.
// One slot stays empty to distinguish full from empty. No heap allocation.
// Head/tail and byte accesses are 8-bit and atomic on ATmega328P.
// Ownership: only one context pushes and only one context pops.
template <uint16_t Capacity>
class ByteRing {
    static_assert(Capacity >= 2 && Capacity <= 256 &&
                  (Capacity & (Capacity - 1)) == 0,
                  "ByteRing capacity must be a power of two from 2 to 256");
public:
    ByteRing() : head_(0), tail_(0) {}

    bool push(uint8_t value) {
        const uint8_t head = head_;
        const uint8_t next = static_cast<uint8_t>((head + 1U) & (Capacity - 1U));
        if (next == tail_) return false;
        data_[head] = value;
        head_ = next;
        return true;
    }

    bool pop(uint8_t& value) {
        const uint8_t tail = tail_;
        if (tail == head_) return false;
        value = data_[tail];
        tail_ = static_cast<uint8_t>((tail + 1U) & (Capacity - 1U));
        return true;
    }

    uint8_t available() const {
        return static_cast<uint8_t>((head_ - tail_) & (Capacity - 1U));
    }

    uint8_t freeSpace() const {
        return static_cast<uint8_t>((Capacity - 1U) - available());
    }

    void reset() { head_ = 0; tail_ = 0; } // Only when producer/consumer are stopped.
private:
    volatile uint8_t data_[Capacity];
    volatile uint8_t head_;
    volatile uint8_t tail_;
};
#endif
