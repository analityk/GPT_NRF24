#ifndef GPT_HOST_TEST
#include "pp_uart_bridge_port.h"
#include "pp_echo_ramp_port.h"
#endif
#include "reliable_input_source.h"

ReliableInputSource::ReliableInputSource(StreamHandle port)
	: port_(port) {
}

void ReliableInputSource::pump() {
#ifdef GPT_HOST_TEST
	const bool shared = port_.sharedInput;
#else
	const bool shared = PP_TEST_TRAFFIC_MODE == PP_TRAFFIC_MODE_UART_BRIDGE;
#endif
	if (shared) {
		return;
	}
	uint8_t value = 0;
	while (ring().freeSpace() != 0 && port_.read(value)) {
		ring().push(value);
	}
}

RetainedByteRing& ReliableInputSource::ring() const {
	return port_.retainedInput();
}

bool ReliableInputSource::hasInput() const {
#ifdef GPT_HOST_TEST
	const bool shared = port_.sharedInput;
#else
	const bool shared = PP_TEST_TRAFFIC_MODE == PP_TRAFFIC_MODE_UART_BRIDGE;
#endif
	return ring().availableToPrepare() != 0 || (!shared && port_.available() != 0);
}
