#include <util/delay.h>
#include "avr_delay.h"

void AvrDelay::us(uint16_t count) const {
	while (count--) {
		_delay_us(1);
	}
}

void AvrDelay::ms(uint8_t count) const {
	while (count--) {
		_delay_ms(1);
	}
}
