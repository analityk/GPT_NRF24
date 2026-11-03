#ifndef GPT_NRF24_TIMER1_CLOCK_H
#define GPT_NRF24_TIMER1_CLOCK_H
#include <stdint.h>

// Dedicated Timer1 timebase: 16MHz / 8 = 2MHz, 0.5us/tick.
// now() is modulo 2^32 and supports sub-35-minute intervals safely.
// A timer ISR extends the hardware 16-bit counter through overflow.
class Timer1Clock {
public:
    static void begin();
    uint32_t now() const;
    static void onOverflow();
private:
    static volatile uint16_t upper_;
};
#endif
