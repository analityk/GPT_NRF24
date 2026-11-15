#include "byte_ring.h"

ByteRing::ByteRing()
	: head_(0),
	  tail_(0) {
}

bool ByteRing::push(uint8_t value) {
	const uint8_t head = head_;
	const uint8_t next = (head + 1U) & (BYTE_RING_CAPACITY - 1U);
	if (next == tail_) {
		return false;
	}
	data_[head] = value;
	head_ = next;
	return true;
}

bool ByteRing::pop(uint8_t& value) {
	const uint8_t tail = tail_;
	if (tail == head_) {
		return false;
	}
	value = data_[tail];
	tail_ = (tail + 1U) & (BYTE_RING_CAPACITY - 1U);
	return true;
}

uint8_t ByteRing::available() const {
	return static_cast<uint8_t>((head_ - tail_) & (BYTE_RING_CAPACITY - 1U));
}

uint8_t ByteRing::freeSpace() const {
	return static_cast<uint8_t>((BYTE_RING_CAPACITY - 1U) - available());
}

void ByteRing::reset() {
	head_ = 0;
	tail_ = 0;
}
