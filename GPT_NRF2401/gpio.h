#ifndef GPIO_H__
#define GPIO_H__
#include <avr/io.h>
class GPIO {
public:
 GPIO();
 GPIO(volatile uint8_t* port, uint8_t pin);
 GPIO(volatile uint8_t* port, volatile uint8_t* ddr, volatile uint8_t* input, uint8_t pin);
 bool valid() const;
 void set(); void clear(); void flip(); void release();
 void i2c_set(); void i2c_clr();
 uint8_t read() const; uint8_t readOpenDrain(); uint8_t readPullUp();
private:
 volatile uint8_t *port_, *ddr_, *input_;
 uint8_t mask_;
};
#endif
