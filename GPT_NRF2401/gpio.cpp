#include "gpio.h"

#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega328__)
#include <avr/io.h>
#endif

#define GPIO_PORT_WIDTH_BITS 8U

GPIO::GPIO(volatile uint8_t* port, uint8_t pin)
	: port_(port), ddr_(0), input_(0),
	mask_(pin < GPIO_PORT_WIDTH_BITS ?
		static_cast<uint8_t>(1U << pin) : 0U){
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega328__)
	if(port == &PORTB){
		ddr_ = &DDRB;
		input_ = &PINB;
	} else if(port == &PORTC){
		ddr_ = &DDRC;
		input_ = &PINC;
	} else if(port == &PORTD){
		ddr_ = &DDRD;
		input_ = &PIND;
	}
#endif
}

GPIO::GPIO(volatile uint8_t* port, volatile uint8_t* ddr,
	volatile uint8_t* input, uint8_t pin)
	: port_(port), ddr_(ddr), input_(input),
	mask_(pin < GPIO_PORT_WIDTH_BITS ?
		static_cast<uint8_t>(1U << pin) : 0U){
}

void GPIO::outputHigh(){
	*port_ |= mask_;
	*ddr_ |= mask_;
}

void GPIO::outputLow(){
	*port_ &= static_cast<uint8_t>(~mask_);
	*ddr_ |= mask_;
}

void GPIO::set(){
	*port_ |= mask_;
}

void GPIO::clear(){
	*port_ &= static_cast<uint8_t>(~mask_);
}

void GPIO::flip(){
	*port_ ^= mask_;
}

void GPIO::release(){
	*ddr_ &= static_cast<uint8_t>(~mask_);
	*port_ &= static_cast<uint8_t>(~mask_);
}

void GPIO::i2c_set(){
	*ddr_ &= static_cast<uint8_t>(~mask_);
	*port_ |= mask_;
}

void GPIO::i2c_clr(){
	*port_ &= static_cast<uint8_t>(~mask_);
	*ddr_ |= mask_;
}

uint8_t GPIO::read() const{
	return static_cast<uint8_t>((*input_ & mask_) != 0U);
}
