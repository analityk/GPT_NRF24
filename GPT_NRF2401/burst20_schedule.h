#ifndef GPT_NRF24_BURST20_SCHEDULE_H
#define GPT_NRF24_BURST20_SCHEDULE_H
#include <stdint.h>

// Portable, tick-based MAC scheduler. No GPIO, SPI, UART, heap or delays.
// All 20ms periods are measured from a leader-owned reference using an
// integer number of Timer1 ticks (2MHz). A follower must first synchronize
// the anchor from a valid, CRC-protected on-air control exchange.
//
// The scheduler only grants transmission rights. The PHY must enforce
// the guard, abort/flush a pending RF transaction before the deadline,
// and switch PTX/PRX roles safely. A REPORT phase reserves a transmission
// OPPORTUNITY; the caller must send at most one report per slot.
class Burst20Schedule {
  public:
	enum class Share : uint8_t { A5_B15 = 1, A10_B10 = 2, A15_B5 = 3 };
	enum class Owner : uint8_t { A = 0, B = 1 };
	enum class Phase : uint8_t { DATA = 0, REPORT = 1, GUARD = 2 };
	enum class Error : uint8_t { NONE = 0, BAD_SHARE = 1, NOT_ALIGNED = 2 };
	static const uint32_t TICKS_PER_US = 2UL;
	static const uint32_t QUANTUM_TICKS = 10000UL; // 5ms
	static const uint32_t CYCLE_TICKS = 40000UL;   // 20ms
	static const uint32_t REPORT_TICKS = 2000UL;   // 1ms reserved
	static const uint32_t GUARD_TICKS = 600UL;	   // 0.3ms reserved

	struct Position {
		Owner owner;
		Phase phase;
		uint16_t cycle;			 // modulo 65536; reset by session handshake
		uint32_t slotRemaining;	 // in 0.5us Timer1 ticks
		uint32_t phaseRemaining; // in 0.5us Timer1 ticks
		uint32_t cycleRemaining;
		Position();
		bool canStart(uint32_t worstCaseTicks) const;
	};

	Burst20Schedule();

	static bool valid(Share share);
	static uint8_t aQuanta(Share share);
	static uint8_t bQuanta(Share share);
	Error align(uint32_t cycleStartTicks, Share share, uint16_t cycle = 0);
	void invalidate();
	bool aligned() const;
	Share share() const;
	uint16_t cycle() const;

	// At most one call per 2^32 Timer1 ticks (~35.8 min) is required to
	// disambiguate a wrapped time counter; regular packet/slot-loop calls
	// are sufficient. This is O(1) even after a long pause.
	Error advance(uint32_t nowTicks);
	Position position(uint32_t nowTicks) const;

  private:
	uint32_t anchor_;
	uint16_t cycle_;
	Share share_;
	bool aligned_;
};
#endif
