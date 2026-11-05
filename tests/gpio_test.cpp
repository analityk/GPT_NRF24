#include "../GPT_NRF2401/gpio.cpp"
#include "../GPT_NRF2401/nrf24.h"

#include <assert.h>
#include <type_traits>

#define GPIO_TEST_PIN 3U
#define GPIO_TEST_BIT static_cast<uint8_t>(1U << GPIO_TEST_PIN)
#define GPIO_TEST_OTHER_PORT_BIT 0x80U
#define GPIO_TEST_OTHER_DDR_BIT 0x20U

static_assert(!std::is_default_constructible<GPIO>::value,
	"GPIO requires explicitly configured registers");

static void testOutputLatch(){
	volatile uint8_t port = GPIO_TEST_OTHER_PORT_BIT;
	volatile uint8_t ddr = GPIO_TEST_OTHER_DDR_BIT;
	volatile uint8_t input = 0;
	GPIO pin(&port, &ddr, &input, GPIO_TEST_PIN);

	pin.outputHigh();
	assert(port == (GPIO_TEST_OTHER_PORT_BIT | GPIO_TEST_BIT));
	assert(ddr == (GPIO_TEST_OTHER_DDR_BIT | GPIO_TEST_BIT));

	pin.clear();
	assert(port == GPIO_TEST_OTHER_PORT_BIT);
	assert(ddr == (GPIO_TEST_OTHER_DDR_BIT | GPIO_TEST_BIT));

	pin.set();
	assert(port == (GPIO_TEST_OTHER_PORT_BIT | GPIO_TEST_BIT));
	assert(ddr == (GPIO_TEST_OTHER_DDR_BIT | GPIO_TEST_BIT));

	pin.flip();
	assert(port == GPIO_TEST_OTHER_PORT_BIT);
	assert(ddr == (GPIO_TEST_OTHER_DDR_BIT | GPIO_TEST_BIT));

	pin.outputLow();
	assert(port == GPIO_TEST_OTHER_PORT_BIT);
	assert(ddr == (GPIO_TEST_OTHER_DDR_BIT | GPIO_TEST_BIT));
}

static void testInputModes(){
	volatile uint8_t port = GPIO_TEST_OTHER_PORT_BIT;
	volatile uint8_t ddr = GPIO_TEST_OTHER_DDR_BIT | GPIO_TEST_BIT;
	volatile uint8_t input = 0;
	GPIO pin(&port, &ddr, &input, GPIO_TEST_PIN);

	pin.release();
	assert(port == GPIO_TEST_OTHER_PORT_BIT);
	assert(ddr == GPIO_TEST_OTHER_DDR_BIT);

	pin.i2c_set();
	assert(port == (GPIO_TEST_OTHER_PORT_BIT | GPIO_TEST_BIT));
	assert(ddr == GPIO_TEST_OTHER_DDR_BIT);

	input = GPIO_TEST_BIT;
	assert(pin.read() == 1U);
	input = 0;
	assert(pin.read() == 0U);
	assert(port == (GPIO_TEST_OTHER_PORT_BIT | GPIO_TEST_BIT));
	assert(ddr == GPIO_TEST_OTHER_DDR_BIT);

	pin.i2c_clr();
	assert(port == GPIO_TEST_OTHER_PORT_BIT);
	assert(ddr == (GPIO_TEST_OTHER_DDR_BIT | GPIO_TEST_BIT));
}

struct TestSpi {
	bool initialized = false;

	void begin(){
		initialized = true;
	}
};

struct TestRadioPin {
	bool configured = false;
	bool high = false;

	void outputHigh(){
		configured = true;
		high = true;
	}

	void outputLow(){
		configured = true;
		high = false;
	}

	void set(){
		assert(configured);
		high = true;
	}

	void clear(){
		assert(configured);
		high = false;
	}
};

static void testRadioPinInitialization(){
	TestSpi spi;
	TestRadioPin ce, csn;
	Nrf24<TestSpi, TestRadioPin> radio(spi, ce, csn);
	radio.begin();
	assert(spi.initialized);
	assert(ce.configured && !ce.high);
	assert(csn.configured && csn.high);
}

int main(){
	testOutputLatch();
	testInputModes();
	testRadioPinInitialization();
}
