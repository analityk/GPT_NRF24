#include "adc0.h"
#include <avr/io.h>
#include <util/delay.h>

static bool conversion(uint16_t& value) {
    ADCSRA |= _BV(ADSC);
    uint16_t timeout=50000;
    while (ADCSRA & _BV(ADSC)) {
        if (--timeout == 0U) return false;
    }
    value=ADC; // Read ADCL then ADCH via avr/io.h combined register.
    return true;
}

bool Adc0::sample(uint16_t& result) {
    DDRC &= static_cast<uint8_t>(~_BV(DDC0));
    PORTC &= static_cast<uint8_t>(~_BV(PORTC0)); // no pullup
    ADMUX = _BV(REFS0); // AVCC reference, ADC0 input, right aligned
    ADCSRB=0;
    DIDR0 |= _BV(ADC0D);
    ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0); // 125 kHz
    _delay_ms(2);
    uint16_t discarded=0;
    if (!conversion(discarded)) return false;
    uint16_t sum=0;
    for (uint8_t i=0; i<8; ++i) {
        uint16_t sample=0;
        if (!conversion(sample)) return false;
        sum=static_cast<uint16_t>(sum+sample);
    }
    result=static_cast<uint16_t>((sum+4U)/8U);
    return true;
}
