#ifndef GPT_HOST_TEST
#include "uart.h"
#endif
#include "pp_echo_ramp_port.h"

PpEchoRampPort::PpEchoRampPort(ConsoleHandle serial)
	: serial_(serial),
	  leader_(false),
	  mode_(Mode::UART_ECHO),
	  periodUs_(START_US),
	  nextDue_(0),
	  nextByte_(0),
	  expectedEcho_(0),
	  generated_(0),
	  pressure_(0),
	  echoed_(0),
	  echoGaps_(0),
	  received_(0),
	  forwarded_(0) {
}

void PpEchoRampPort::configure(bool leader, uint32_t nowTicks, Mode mode) {
	leader_ = leader;
	mode_ = mode;
	pending_.reset();
	periodUs_ = (mode_ == Mode::RADIO_STRESS && leader_) ? 0 : START_US;
	nextDue_ = nowTicks + periodTicks();
	nextByte_ = 0;
	expectedEcho_ = 0;
	generated_ = 0;
	pressure_ = 0;
	echoed_ = 0;
	echoGaps_ = 0;
	received_ = 0;
	forwarded_ = 0;
}

void PpEchoRampPort::service(uint32_t nowTicks, bool producerPermitted, uint8_t maxProduced) {
	if (!leader_) {
		return;
	}
	uint8_t command = 0;
	while (serial_.read(command)) {
		if (command == 's') {
			if (periodUs_ >= STEP_US) {
				periodUs_ = periodUs_ - STEP_US;
			}
			nextDue_ = nowTicks + periodTicks();
		} else if (command == 'w') {
			if (periodUs_ <= START_US - STEP_US) {
				periodUs_ = periodUs_ + STEP_US;
			}
			nextDue_ = nowTicks + periodTicks();
		}
	}
	// Pause at the *producer* boundary. Existing queued bytes, sequence
	// number and timer phase are preserved across a temporary RF outage.
	// Avoid a catch-up burst when the link eventually recovers.
	if (!producerPermitted) {
		nextDue_ = nowTicks + periodTicks();
		return;
	}
	if (periodUs_ == 0) {
		while (pending_.available() < HIGH_WATER && maxProduced != 0) {
			produce();
			--maxProduced;
		}
		++pressure_; // offered rate is beyond the bounded queue limit
		return;
	}
	// Catch up after a long radio turn, but never exceed the high-water
	// mark. Reset the due time on pressure to avoid a deferred burst.
	while (maxProduced != 0 && reached(nowTicks, nextDue_)) {
		if (pending_.available() >= HIGH_WATER) {
			++pressure_;
			nextDue_ = nowTicks + periodTicks();
			break;
		}
		produce();
		--maxProduced;
		nextDue_ += periodTicks();
	}
}

uint8_t PpEchoRampPort::available() const {
	return pending_.available();
}

uint8_t PpEchoRampPort::inputFreeSpace() const {
	return pending_.freeSpace();
}

bool PpEchoRampPort::read(uint8_t& b) {
	if (!pending_.pop(b)) {
		return false;
	}
	if (!leader_) {
		++forwarded_;
	}
	return true;
}

uint8_t PpEchoRampPort::freeSpace() const {
	// In stress mode the radio echo is accepted in RAM, not into the
	// slow console TX ring. Its credit must not depend on UART speed.
	return leader_ ? (mode_ == Mode::RADIO_STRESS ? 127U : serial_.freeSpace())
				   : pending_.freeSpace();
}

bool PpEchoRampPort::writeAll(const uint8_t* bytes, uint8_t count) {
	if (count && !bytes) {
		return false;
	}
	if (leader_) {
		// UART_ECHO requires atomic UART TX acceptance; RADIO_STRESS
		// consumes the bytes in RAM without UART backpressure. On a
		// mismatch, resynchronize to count one sequence discontinuity.
		if (mode_ == Mode::UART_ECHO && !serial_.writeAll(bytes, count)) {
			return false;
		}
		for (uint8_t i = 0; i < count; ++i) {
			if (bytes[i] != expectedEcho_) {
				++echoGaps_;
			}
			expectedEcho_ = bytes[i] + 1U;
			++echoed_;
		}
		return true;
	}
	if (pending_.freeSpace() < count) {
		return false;
	}
	for (uint8_t i = 0; i < count; ++i) {
		(void)pending_.push(bytes[i]);
	} // checked atomically as a group
	received_ += count;
	return true;
}

uint16_t PpEchoRampPort::periodUs() const {
	return periodUs_;
}

uint32_t PpEchoRampPort::generated() const {
	return generated_;
}

uint32_t PpEchoRampPort::pressureEvents() const {
	return pressure_;
}

uint32_t PpEchoRampPort::echoed() const {
	return echoed_;
}

uint32_t PpEchoRampPort::echoGaps() const {
	return echoGaps_;
}

uint32_t PpEchoRampPort::received() const {
	return received_;
}

uint32_t PpEchoRampPort::forwarded() const {
	return forwarded_;
}

bool PpEchoRampPort::reached(uint32_t now, uint32_t deadline) {
	return static_cast<uint32_t>(now - deadline) < 0x80000000UL;
}

uint32_t PpEchoRampPort::periodTicks() const {
	return static_cast<uint32_t>(periodUs_) * PP_ECHO_TICKS_PER_US;
}

void PpEchoRampPort::produce() {
	// All calls have checked the bounded queue's high-water mark.
	if (pending_.push(nextByte_)) {
		++nextByte_; // uint8_t wraps from 255 back to zero
		++generated_;
	}
}

RetainedByteRing& PpEchoRampPort::retainedInput() {
	return retained_;
}

#ifdef GPT_HOST_TEST
PpEchoRampPort::operator BytePort() {
	BytePort port = {};
	port.context = this;
	port.retained = &retained_;
	port.sharedInput = false;
	port.readFn = [](void* c, uint8_t& value) {
		return static_cast<PpEchoRampPort*>(c)->read(value);
	};
	port.availableFn = [](void* c) {
		return static_cast<PpEchoRampPort*>(c)->available();
	};
	port.freeSpaceFn = [](void* c) {
		return static_cast<PpEchoRampPort*>(c)->freeSpace();
	};
	port.writeAllFn = [](void* c, const uint8_t* bytes, uint8_t count) {
		return static_cast<PpEchoRampPort*>(c)->writeAll(bytes, count);
	};
	return port;
}
#endif
