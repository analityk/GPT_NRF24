#ifndef GPT_NRF24_PP_STRESS_TELEMETRY_H
#define GPT_NRF24_PP_STRESS_TELEMETRY_H
#include "hardware_bindings.h"

#include <stdint.h>
#include "foreground_ports.h"

// Per-device cumulative counters. Source and delivered bytes have different
// meanings by role: A generated/echoed; B forwarded/accepted into the echo
// queue. Neither is measured via the physical UART. All fields except SID
// and state must be monotonic modulo their unsigned integer width.
struct PpStressSnapshot {
	uint32_t sourceBytes, deliveredBytes, hardwareRetries, maxRetries;
	uint32_t echoGaps;
	uint16_t appRetries, duplicates, badFrames, blocked;
	uint16_t radioErrors, recoveries, sid;
	uint16_t acknowledgedBytes, linkGaps, linkStale, rxFifoFull, ackFifoFull;
	uint8_t outstanding;
	bool active;
	PpStressSnapshot();
};

struct PpStressSample {
	uint32_t cps, sourceCps, hardwareRetries, maxRetries, echoGaps;
	uint16_t appRetries, duplicates, badFrames, blocked;
	uint16_t radioErrors, recoveries, sid;
	uint16_t acknowledgedBytes, linkGaps, linkStale, rxFifoFull, ackFifoFull;
	uint8_t outstanding;
	bool active;
};

// Reusable one-second measurement window for a 2 MHz modulo-2^32 clock.
// The actual elapsed time, not an assumed exact one-second timer interrupt,
// is used to normalize CPS. All differences are wrap-safe; 16-bit transport
// counters are sampled frequently enough not to wrap more than once/window.
class PpStressWindow {
  public:
	PpStressWindow();
	void start(uint32_t ticks, const PpStressSnapshot& initial);
	bool take(uint32_t now, const PpStressSnapshot& current, PpStressSample& sample);

  private:
	static uint32_t rate(uint32_t bytes, uint32_t elapsedMs);
	uint32_t lastTicks_;
	PpStressSnapshot last_;
	bool started_;
};

// Atomic, bounded ASCII telemetry output. A single line always fits the
// 127-byte usable UART ring if it is emitted, and never interleaves a binary
// transport segment. No printf/heap/AVR dependency. A failure drops only a
// diagnostic line, never a radio byte or a protocol ACK.
class PpStressLine {
  public:
	PpStressLine();
	void word(const char* value);
	void decimal(uint32_t value);
	uint8_t length() const;

	bool emit(ConsoleHandle serial) const;

  private:
	void character(char c);
	char data_[127];
	uint8_t size_;
	bool valid_;
};

bool printPpStressLine(ConsoleHandle serial, bool leader, const PpStressSample& s);

#endif
