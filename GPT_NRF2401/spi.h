#ifndef GPT_NRF24_SPI_H
#define GPT_NRF24_SPI_H

#include <stdint.h>

// ATmega328P hardware SPI master (mode 0, MSB first, F_CPU/16 = 1 MHz).
// One caller at a time. The UART interrupt does not touch this peripheral.
// Every byte transfer has a finite polling limit.
class SpiMaster {
public:
    SpiMaster() : timeouts_(0) {}
    enum class Speed : uint8_t { MHz1=0, MHz8=1 };
    void begin();
    void setSpeed(Speed speed);
    bool transfer(uint8_t outgoing, uint8_t& incoming);
    uint16_t timeouts() const { return timeouts_; }
private:
    uint16_t timeouts_;
};
#endif
