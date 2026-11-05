#ifndef GPIO_H__
#define GPIO_H__

#include <stdint.h>

// The caller supplies a real hardware port. Invalid register addresses are
// configuration errors, not conditions to test on every pin transition.
class GPIO {
public:
	GPIO() = delete;
	GPIO(volatile uint8_t* port, uint8_t pin);
	GPIO(volatile uint8_t* port, volatile uint8_t* ddr,
		volatile uint8_t* input, uint8_t pin);

	// Configure output once, loading the requested level BEFORE enabling DDR.
	void outputHigh();
	void outputLow();

	// Fast latch operations. They never inspect or modify the direction.
	void set();
	void clear();
	void flip();

	// Explicit mode transitions for high-impedance and open-drain uses.
	void release();
	void i2c_set();
	void i2c_clr();

	// Sample the input register without changing the pin configuration.
	uint8_t read() const;

private:
	volatile uint8_t *port_, *ddr_, *input_;
	uint8_t mask_;
};

#endif
