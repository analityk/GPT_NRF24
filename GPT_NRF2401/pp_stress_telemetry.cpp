#ifndef GPT_HOST_TEST
#include "uart.h"
#endif
#include "pp_stress_telemetry.h"

PpStressSnapshot::PpStressSnapshot()
	: sourceBytes(0),
	  deliveredBytes(0),
	  hardwareRetries(0),
	  maxRetries(0),
	  echoGaps(0),
	  appRetries(0),
	  duplicates(0),
	  badFrames(0),
	  blocked(0),
	  radioErrors(0),
	  recoveries(0),
	  sid(0),
	  acknowledgedBytes(0),
	  linkGaps(0),
	  linkStale(0),
	  rxFifoFull(0),
	  ackFifoFull(0),
	  outstanding(0),
	  active(false) {
}

PpStressWindow::PpStressWindow()
	: lastTicks_(0),
	  started_(false) {
}

void PpStressWindow::start(uint32_t ticks, const PpStressSnapshot& initial) {
	lastTicks_ = ticks;
	last_ = initial;
	started_ = true;
}

bool PpStressWindow::take(uint32_t now, const PpStressSnapshot& current, PpStressSample& sample) {
	if (!started_) {
		start(now, current);
		return false;
	}
	const uint32_t elapsed = now - lastTicks_;
	if (elapsed < 2000000UL) {
		return false;
	}
	const uint32_t elapsedMs = elapsed / 2000UL;
	sample.cps = rate(current.deliveredBytes - last_.deliveredBytes, elapsedMs);
	sample.sourceCps = rate(current.sourceBytes - last_.sourceBytes, elapsedMs);
	sample.hardwareRetries = current.hardwareRetries - last_.hardwareRetries;
	sample.maxRetries = current.maxRetries - last_.maxRetries;
	sample.echoGaps = current.echoGaps - last_.echoGaps;
	sample.appRetries = current.appRetries - last_.appRetries;
	sample.duplicates = current.duplicates - last_.duplicates;
	sample.badFrames = current.badFrames - last_.badFrames;
	sample.blocked = current.blocked - last_.blocked;
	sample.radioErrors = current.radioErrors - last_.radioErrors;
	sample.recoveries = current.recoveries - last_.recoveries;
	sample.acknowledgedBytes = current.acknowledgedBytes - last_.acknowledgedBytes;
	// Link sequence counters are scoped to one negotiated session,
	// unlike the lifetime counters in the PHY and transport.
	sample.linkGaps = (current.sid == last_.sid)
						  ? static_cast<uint16_t>(current.linkGaps - last_.linkGaps)
						  : current.linkGaps;
	sample.linkStale = (current.sid == last_.sid)
						   ? static_cast<uint16_t>(current.linkStale - last_.linkStale)
						   : current.linkStale;
	sample.rxFifoFull = current.rxFifoFull - last_.rxFifoFull;
	sample.ackFifoFull = current.ackFifoFull - last_.ackFifoFull;
	sample.outstanding = current.outstanding;
	sample.sid = current.sid;
	sample.active = current.active;
	lastTicks_ = now;
	last_ = current;
	return true;
}

uint32_t PpStressWindow::rate(uint32_t bytes, uint32_t elapsedMs) {
	// Dividing before multiplying prevents 32-bit overflow without
	// costly 64-bit division on the ATmega.
	return (bytes / elapsedMs) * 1000UL + ((bytes % elapsedMs) * 1000UL) / elapsedMs;
}

PpStressLine::PpStressLine()
	: size_(0),
	  valid_(true) {
}

void PpStressLine::word(const char* value) {
	if (!value) {
		valid_ = false;
		return;
	}
	while (*value) {
		character(*value++);
	}
}

void PpStressLine::decimal(uint32_t value) {
	char digits[10];
	uint8_t used = 0;
	do {
		digits[used++] = static_cast<char>('0' + value % 10UL);
		value /= 10UL;
	} while (value);
	while (used) {
		character(digits[--used]);
	}
}

uint8_t PpStressLine::length() const {
	return size_;
}

bool PpStressLine::emit(ConsoleHandle serial) const {
	return valid_ && serial.writeAll(reinterpret_cast<const uint8_t*>(data_), size_);
}

void PpStressLine::character(char c) {
	if (size_ < sizeof(data_)) {
		data_[size_++] = c;
	} else {
		valid_ = false;
	}
}

bool printPpStressLine(ConsoleHandle serial, bool leader, const PpStressSample& s) {
	PpStressLine line;
	line.word(leader ? "A cps=" : "B cps=");
	line.decimal(s.cps);
	line.word(leader ? " gen=" : " fwd=");
	line.decimal(s.sourceCps);
	if (leader) {
		line.word(" ack=");
		line.decimal(s.acknowledgedBytes);
		line.word(" win=");
		line.decimal(s.outstanding);
		line.word(" arc=");
		line.decimal(s.hardwareRetries);
		line.word(" max=");
		line.decimal(s.maxRetries);
	}
	line.word(" ret=");
	line.decimal(s.appRetries);
	if (!leader) {
		line.word(" dup=");
		line.decimal(s.duplicates);
	}
	line.word(" bad=");
	line.decimal(s.badFrames);
	if (!leader) {
		line.word(" lg=");
		line.decimal(s.linkGaps);
		line.word(" ls=");
		line.decimal(s.linkStale);
		line.word(" rf=");
		line.decimal(s.rxFifoFull);
		line.word(" af=");
		line.decimal(s.ackFifoFull);
	}
	if (!leader) {
		line.word(" blk=");
		line.decimal(s.blocked);
	}
	if (leader) {
		line.word(" gap=");
		line.decimal(s.echoGaps);
	}
	line.word(" err=");
	line.decimal(s.radioErrors);
	line.word(" rec=");
	line.decimal(s.recoveries);
	line.word(" sid=");
	line.decimal(s.sid);
	line.word(" up=");
	line.decimal(s.active ? 1U : 0U);
	line.word("\r\n");
	return line.emit(serial);
}
