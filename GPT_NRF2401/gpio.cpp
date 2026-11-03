#include "gpio.h"
GPIO::GPIO():port_(0),ddr_(0),input_(0),mask_(0){}
GPIO::GPIO(volatile uint8_t* port,uint8_t pin):port_(port),ddr_(0),input_(0),mask_(pin<8?(uint8_t)(1U<<pin):0){
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega328__)
 if(port==&PORTB){ddr_=&DDRB;input_=&PINB;}
 else if(port==&PORTC){ddr_=&DDRC;input_=&PINC;}
 else if(port==&PORTD){ddr_=&DDRD;input_=&PIND;}
#endif
}
GPIO::GPIO(volatile uint8_t* port,volatile uint8_t* ddr,volatile uint8_t* input,uint8_t pin):port_(port),ddr_(ddr),input_(input),mask_(pin<8?(uint8_t)(1U<<pin):0){}
bool GPIO::valid() const{return port_&&ddr_&&input_&&mask_;}
void GPIO::set(){if(!valid())return;*port_|=mask_;*ddr_|=mask_;}
void GPIO::clear(){if(!valid())return;*port_&=(uint8_t)~mask_;*ddr_|=mask_;}
void GPIO::flip(){if(!valid())return;*port_^=mask_;*ddr_|=mask_;}
void GPIO::release(){if(!valid())return;*port_&=(uint8_t)~mask_;*ddr_&=(uint8_t)~mask_;}
void GPIO::i2c_set(){if(!valid())return;*ddr_&=(uint8_t)~mask_;*port_|=mask_;}
void GPIO::i2c_clr(){if(!valid())return;*port_&=(uint8_t)~mask_;*ddr_|=mask_;}
uint8_t GPIO::read() const{return valid()&&(*input_&mask_)?1:0;}
uint8_t GPIO::readOpenDrain(){release();return read();}
uint8_t GPIO::readPullUp(){i2c_set();return read();}
