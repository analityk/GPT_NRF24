#ifndef GPT_NRF24_PP_SHORT_PROBE_H
#define GPT_NRF24_PP_SHORT_PROBE_H
#include "hardware_bindings.h"

#include <stdint.h>
#include "foreground_ports.h"

// The hardware 16-bit timer wraps naturally. A single measured operation
// must take less than one timer rollover; longer deadlines still use
// Timer1Clock::now(). The window sum is wider than individual samples.
#define PP_SHORT_PROBE_TICKS_PER_US 2U
#define PP_SHORT_PROBE_MIN_SAMPLES 32U
#define PP_SHORT_PROBE_MAX_SAMPLES 1024U

// Reusable two-snapshot timer for short, nonblocking code sections.
class ShortSpanTimer {
  public:
	explicit ShortSpanTimer(ClockHandle clock);

	void stop();

	bool done() const;

	uint16_t elapsedTicks() const;

  private:
	ClockHandle clock_;
	uint16_t begin_, elapsed_;
	bool done_;
};

// Adapter to the portable stream's optional sourceReady/frameEncoded hooks.
class PpShortSourceTrace {
  public:
	operator TracePort();

  public:
	explicit PpShortSourceTrace(ClockHandle clock);

	void sourceReady();

	void frameEncoded();

	bool sampled() const;

	uint16_t sourceTicks() const;

	uint16_t encodeTicks() const;

  private:
	ShortSpanTimer timer_;
};

// 32-bit aggregation runs once per segment; division runs only in the
// low-priority report. No 32-bit timestamp operations in the hot section.
class PpShortProbeWindow {
  public:
	struct Snapshot {
		uint16_t count, meanUs, minUs, maxUs;
	};

	PpShortProbeWindow();

	void observe(uint16_t elapsedTicks);

	bool sample(Snapshot& result) const;

	void clear();

  private:
	uint16_t samples_;
	uint32_t sumTicks_;
	uint16_t minTicks_, maxTicks_;
};

#endif
