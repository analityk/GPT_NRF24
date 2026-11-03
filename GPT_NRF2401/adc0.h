#ifndef GPT_NRF24_ADC0_H
#define GPT_NRF24_ADC0_H

#include <stdint.h>

// Independent ADC0 sampling utility, no heap or global side effects beyond
// ADC0 configuration. Returns a latched power-on value on first invocation.
class Adc0 {
public:
    static bool sample(uint16_t& result);
    static uint8_t classForM1(uint16_t adc) { return adc < 512U ? 0U : 1U; }
};
#endif
