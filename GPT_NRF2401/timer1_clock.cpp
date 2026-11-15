#include "timer1_clock.h"
#include "board_clock.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
volatile uint16_t timer1_overflow_count = 0;

void Timer1Clock::begin() {
	TCCR1A = 0;
	TCCR1B = 0;
	TCNT1 = 0;
	timer1_overflow_count = 0;
	TIFR1 = _BV(TOV1);
	TIMSK1 |= _BV(TOIE1);
	TCCR1B = _BV(CS11); // prescale 8 => 2MHz at 16MHz
}

uint16_t Timer1Clock::counter16() const {
	return TCNT1;
}

uint32_t Timer1Clock::now() const {
	uint16_t hi = 0, lo = 0;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		hi = timer1_overflow_count;
		lo = TCNT1;
		if (TIFR1 & _BV(TOV1)) {
			// Overflow pending while interrupts were disabled.
			++hi;
			lo = TCNT1;
		}
	}
	return (static_cast<uint32_t>(hi) << 16) | lo;
}

ISR(TIMER1_OVF_vect) {
	++timer1_overflow_count;
}
