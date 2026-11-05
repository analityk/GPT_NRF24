#ifndef GPT_NRF24_PP_UART_BRIDGE_PORT_H
#define GPT_NRF24_PP_UART_BRIDGE_PORT_H

#include <stdint.h>

// Binary-transparent data source/sink for UartReliableStream.
// The UART ISR owns its RX producer and TX consumer; the protocol uses
// the opposite ends of these same bounded buffers. No extra queue,
// formatting, timebase or radio-specific state is introduced here.
// Console::writeAll must preserve atomic all-or-none acceptance.
template<class Console>
class PpUartBridgePort {
public:
	explicit PpUartBridgePort(Console& console):console_(console){
	}

	RetainedByteRing& retainedInput(){
		return console_.retainedInput();
	}

	const RetainedByteRing& retainedInput() const{
		return console_.retainedInput();
	}

	uint8_t inputFreeSpace() const{
		return console_.retainedInput().freeSpace();
	}

	bool pauseHostInput() const{
		return console_.retainedInput().pauseRequested();
	}

	uint8_t available() const{
		return console_.available();
	}

	bool read(uint8_t& value){
		return console_.read(value);
	}

	uint8_t freeSpace() const{
		return console_.freeSpace();
	}

	bool writeAll(const uint8_t* bytes,uint8_t count){
		return console_.writeAll(bytes,count);
	}

private:
	Console& console_;
};

#endif
