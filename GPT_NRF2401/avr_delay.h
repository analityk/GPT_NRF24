#ifndef GPT_NRF24_AVR_DELAY_H
#define GPT_NRF24_AVR_DELAY_H
#ifndef F_CPU
#define F_CPU 16000000UL
#endif
#if F_CPU != 16000000UL
#error "RF-Nano test requires 16 MHz F_CPU"
#endif
#include <stdint.h>
#include <util/delay.h>

// AVR adapter for testable Nrf24Phy timing requirements.
class AvrDelay {
public:
    void us(uint16_t count) const {
        while (count--) _delay_us(1);
    }
    void ms(uint8_t count) const {
        while (count--) _delay_ms(1);
    }
};
#endif
