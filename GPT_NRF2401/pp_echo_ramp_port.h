#ifndef GPT_NRF24_PP_ECHO_RAMP_PORT_H
#define GPT_NRF24_PP_ECHO_RAMP_PORT_H
#include "hardware_bindings.h"

#include <stdint.h>
#include "foreground_ports.h"
#include "byte_ring.h"
#define PP_ECHO_TICKS_PER_US 2UL

// Portable diagnostic data source/sink for the EXISTING paired stream.
// A: time-paced 00,01,...,FF source -> radio; radio echo either to binary
// UART TX (interactive ramp) or verified silently in RAM (RF saturation).
// B: radio RX -> bounded local queue -> radio ACK Payload in the next turn.
// Physical PP/session/Go-back-N mechanics remain in UartReliableStream.
// No diagnostic text is mixed with the binary UART output.
//
// The supplied monotonic clock has PP_ECHO_TICKS_PER_US ticks per microsecond. This
// class owns neither the hardware timer nor the RF transceiver. service()
// must run regularly (within 2^31 ticks), including during idle RF turns.
class PpEchoRampPort {
  public:
#ifdef GPT_HOST_TEST
	operator BytePort();
#endif
	RetainedByteRing& retainedInput();

  public:
	enum class Mode : uint8_t { UART_ECHO, RADIO_STRESS };

	static const uint16_t START_US = 10000U;
	static const uint16_t STEP_US = 100U;
	static const uint8_t HIGH_WATER = 120U; // 120 of 127 usable bytes

	explicit PpEchoRampPort(ConsoleHandle serial);

	void configure(bool leader, uint32_t nowTicks, Mode mode = Mode::UART_ECHO);

	// On A, only 's' and 'w' from UART are commands, never stream payload.
	// A zero interval means "as fast as the bounded radio source queue can
	// drain"; an overfull source never skips a number or overwrites data.
	// maxProduced allows a cooperative radio scheduler to cap one slice of
	// producer work; the default preserves the ordinary full-refill API.
	// No bytes are skipped: later calls resume at the same nextByte_.
	void service(
		uint32_t nowTicks, bool producerPermitted = true, uint8_t maxProduced = HIGH_WATER);

	// Port interface required by UartReliableStream.
	uint8_t available() const;
	uint8_t inputFreeSpace() const;
	bool read(uint8_t& b);
	uint8_t freeSpace() const;
	bool writeAll(const uint8_t* bytes, uint8_t count);

	// Debugger-visible observations; not sent to the binary UART.
	uint16_t periodUs() const;
	uint32_t generated() const;
	uint32_t pressureEvents() const;
	uint32_t echoed() const;
	uint32_t echoGaps() const;
	uint32_t received() const;
	uint32_t forwarded() const;

  private:
	static bool reached(uint32_t now, uint32_t deadline);
	uint32_t periodTicks() const;
	void produce();
	ConsoleHandle serial_;
	ByteRing pending_;
	RetainedByteRing retained_;
	bool leader_;
	Mode mode_;
	uint16_t periodUs_;
	uint32_t nextDue_;
	uint8_t nextByte_, expectedEcho_;
	uint32_t generated_, pressure_, echoed_, echoGaps_;
	uint32_t received_, forwarded_;
};

#endif
