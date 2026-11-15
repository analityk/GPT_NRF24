#ifndef GPT_HOST_TEST
#include "gpio.h"
#include "avr_delay.h"
#endif
#include "profile_select.h"

ProfileSelect::Profile ProfileSelect::fromStableLevel(bool high) {
	return high ? Profile::PP : Profile::BURST20;
}

ProfileSelect::Profile ProfileSelect::latch(PinHandle pin, DelayHandle delay) {
	pin.i2c_set(); // digital input with internal pull-up
	delay.us(50);
	const uint8_t first = pin.read();
	delay.us(50);
	const uint8_t second = pin.read();
	return (first == second) ? fromStableLevel(first != 0U) : Profile::PP;
}
