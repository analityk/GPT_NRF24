#ifndef GPT_NRF24_AVR_DELAY_H
#define GPT_NRF24_AVR_DELAY_H
#include "board_clock.h"
#include <stdint.h>
#include "foreground_ports.h"

// AVR adapter for testable Nrf24Phy timing requirements.
class AvrDelay {
  public:
	void us(uint16_t count) const;
	void ms(uint8_t count) const;
};
#endif
