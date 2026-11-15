#include "retained_byte_ring.h"

RetainedByteRing::RetainedByteRing()
	: write_position(0),
	  prepared_position(0),
	  acknowledged_position(0),
	  pause_(false) {
}

bool RetainedByteRing::push(uint8_t byte) {
	const uint8_t next = write_position + 1U;
	if (next == acknowledged_position) {
		return false;
	}
	bytes[write_position] = byte;
	write_position = next;
	return true;
}

bool RetainedByteRing::pop(uint8_t& byte) {
	if (prepared_position != acknowledged_position || write_position == acknowledged_position) {
		return false;
	}
	byte = bytes[acknowledged_position];
	prepared_position = prepared_position + 1U;
	acknowledge(1U);
	return true;
}

uint8_t RetainedByteRing::available() const {
	return static_cast<uint8_t>(write_position - acknowledged_position);
}

uint8_t RetainedByteRing::availableToPrepare() const {
	return static_cast<uint8_t>(write_position - prepared_position);
}

uint8_t RetainedByteRing::freeSpace() const {
	return static_cast<uint8_t>(RETAINED_RING_MAX_BYTES - available());
}

uint8_t RetainedByteRing::prepared() const {
	return prepared_position;
}

uint8_t RetainedByteRing::at(uint8_t index) const {
	return bytes[index];
}

void RetainedByteRing::markPrepared(uint8_t count) {
	prepared_position = prepared_position + count;
}

void RetainedByteRing::acknowledge(uint8_t count) {
	acknowledged_position = acknowledged_position + count;
	if (freeSpace() >= RETAINED_RING_RESUME_FREE_BYTES) {
		pause_ = false;
	}
}

bool RetainedByteRing::pauseRequested() const {
	const uint8_t free = freeSpace();
	if (free <= RETAINED_RING_PAUSE_FREE_BYTES) {
		pause_ = true;
	} else if (free >= RETAINED_RING_RESUME_FREE_BYTES) {
		pause_ = false;
	}
	return pause_;
}

void RetainedByteRing::reset() {
	write_position = 0;
	prepared_position = 0;
	acknowledged_position = 0;
	pause_ = false;
}
