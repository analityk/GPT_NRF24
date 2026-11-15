#ifndef GPT_NRF24_PP_CYCLE_TIMING_H
#define GPT_NRF24_PP_CYCLE_TIMING_H
#include "hardware_bindings.h"

#include <stdint.h>
#include "foreground_ports.h"
#include "nrf24_ack_phy.h"

// A portable, bounded measurement window. The five PHY components are
// disjoint portions of an exchange. Background work overlaps radio wait,
// whereas preparation and protocol processing are diagnostic subsets of
// the CPU time between exchanges. All inputs are 2 MHz Timer1 ticks.
#define PP_TIMING_TICKS_PER_US 2UL
#define PP_TIMING_MAX_SPAN_TICKS 0xFFFFUL
#define PP_TIMING_MAX_CYCLE_TICKS 40000UL
#define PP_TIMING_MAX_SAMPLES 1024U
#define PP_TIMING_MIN_SAMPLES 32U

class PpCycleTimingWindow {
	// Old callers may supply only the five high-level PHY phases.
	struct EmptyBreakdown {
		uint16_t pollCount, pollSpiTicks, observeTicks, fifoTicks, payloadTicks, irqTicks;
		EmptyBreakdown();
	};

  public:
	struct Average {
		uint16_t samples;
		uint16_t cycleUs, loadUs, guardUs, ceUs, waitUs, readUs;
		uint16_t cpuUs, prepareUs, protocolUs, protocolOverlapUs, backgroundUs;
		uint16_t polls, pollSpiUs, observeUs, fifoUs, payloadUs, irqUs;
		uint16_t sourceUs, encodeUs, readOtherUs, prepareOtherUs, cpuOtherUs;
		Average();
	};

	PpCycleTimingWindow();

	static uint16_t clampTicks(uint32_t elapsed);

	// The previous and current exchange both need to be full, healthy DATA
	// turns. A reset, short CONTROL, timeout or missing ACK breaks continuity
	// so that rekey time cannot be mislabeled as normal RF timing.
	void observe(uint32_t completed, const NrfAckPayloadPhy::Timing& t, uint16_t prepareTicks,
		uint16_t protocolTicks, uint16_t backgroundTicks, bool eligible);

	void observe(uint32_t completed, const NrfAckPayloadPhy::Timing& t, uint16_t prepareTicks,
		uint16_t protocolTicks, uint16_t backgroundTicks, uint16_t sourceTicks,
		uint16_t encodeTicks, bool eligible);

	// Protocol handling of the previous ACK can run while the current
	// RF frame is on air. These ticks are a subset of protocolTicks AND
	// the RF wait, so they are not part of exclusive CPU idle time.
	void observe(uint32_t completed, const NrfAckPayloadPhy::Timing& t, uint16_t prepareTicks,
		uint16_t protocolTicks, uint16_t backgroundTicks, uint16_t sourceTicks,
		uint16_t encodeTicks, uint16_t protocolOverlapTicks, bool eligible);

	void observeDetailed(uint32_t completed, const NrfAckPayloadPhy::Timing& t,
		const NrfAckPayloadPhy::Timing& detail, uint16_t prepareTicks, uint16_t protocolTicks,
		uint16_t backgroundTicks, uint16_t sourceTicks, uint16_t encodeTicks,
		uint16_t protocolOverlapTicks, bool eligible);

	// Read without consuming. A full UART TX ring may reject the line;
	// retrying later must preserve the measurements instead of losing them.
	bool sample(Average& out) const;
	void clearWindow();
	bool take(Average& out);

  private:
	enum Field : uint8_t {
		CYCLE,
		LOAD,
		GUARD,
		CE,
		WAIT,
		READ,
		CPU,
		PREPARE,
		PROTOCOL,
		PROTOCOL_OVERLAP,
		BACKGROUND,
		POLL_COUNT,
		POLL_SPI,
		OBSERVE,
		FIFO,
		PAYLOAD,
		IRQ,
		SOURCE,
		ENCODE,
		FIELD_COUNT
	};

	uint16_t mean(Field field) const;
	uint16_t meanCount(Field field) const;
	static uint16_t remainder(
		uint16_t total, uint16_t a, uint16_t b, uint16_t c = 0, uint16_t d = 0);
	void resetSums();
	uint32_t sums_[FIELD_COUNT];
	uint32_t lastDone_;
	uint16_t samples_;
	bool previous_;
};

// Reusable optional timer adapter for UartReliableStream::makeTraced().
// Disabled probes compile to empty callbacks and do not access the clock.
class PpFrameMakeTrace {
  public:
	operator TracePort();

  public:
	PpFrameMakeTrace(ClockHandle clock, uint32_t started, bool enabled = true);
	void sourceReady();
	void frameEncoded();
	uint16_t sourceTicks() const;
	uint16_t encodeTicks() const;

  private:
	bool enabled_;
	ClockHandle clock_;
	uint32_t started_;
	uint16_t sourceTicks_, encodeTicks_;
};

#endif
