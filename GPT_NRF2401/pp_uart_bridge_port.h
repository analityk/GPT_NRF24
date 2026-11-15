#ifndef GPT_NRF24_PP_UART_BRIDGE_PORT_H
#define GPT_NRF24_PP_UART_BRIDGE_PORT_H
#include "hardware_bindings.h"

#include <stdint.h>
#include "foreground_ports.h"

// Binary-transparent data source/sink for UartReliableStream.
// The UART ISR owns its RX producer and TX consumer; the protocol uses
// the opposite ends of these same bounded buffers. No extra queue,
// formatting, timebase or radio-specific state is introduced here.
// Console::writeAll must preserve atomic all-or-none acceptance.
class PpUartBridgePort {
  public:
#ifdef GPT_HOST_TEST
	operator BytePort();
#endif
  public:
	explicit PpUartBridgePort(ConsoleHandle console);

	RetainedByteRing& retainedInput();

	const RetainedByteRing& retainedInput() const;

	uint8_t inputFreeSpace() const;

	bool pauseHostInput() const;

	uint8_t available() const;

	bool read(uint8_t& value);

	uint8_t freeSpace() const;

	bool writeAll(const uint8_t* bytes, uint8_t count);

  private:
	ConsoleHandle console_;
};

#endif
