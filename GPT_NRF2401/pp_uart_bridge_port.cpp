#ifndef GPT_HOST_TEST
#include "uart.h"
#endif
#include "pp_uart_bridge_port.h"

PpUartBridgePort::PpUartBridgePort(ConsoleHandle console)
	: console_(console) {
}

RetainedByteRing& PpUartBridgePort::retainedInput() {
	return console_.retainedInput();
}

const RetainedByteRing& PpUartBridgePort::retainedInput() const {
	return console_.retainedInput();
}

uint8_t PpUartBridgePort::inputFreeSpace() const {
	return console_.retainedInput().freeSpace();
}

bool PpUartBridgePort::pauseHostInput() const {
	return console_.retainedInput().pauseRequested();
}

uint8_t PpUartBridgePort::available() const {
	return console_.available();
}

bool PpUartBridgePort::read(uint8_t& value) {
	return console_.read(value);
}

uint8_t PpUartBridgePort::freeSpace() const {
	return console_.freeSpace();
}

bool PpUartBridgePort::writeAll(const uint8_t* bytes, uint8_t count) {
	return console_.writeAll(bytes, count);
}

#ifdef GPT_HOST_TEST
PpUartBridgePort::operator BytePort() {
	return console_;
}
#endif
