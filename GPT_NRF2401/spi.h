#ifndef GPT_NRF24_SPI_H
#define GPT_NRF24_SPI_H

#include <stdint.h>

#define SPI_MASTER_BYTE_TIMEOUT_POLLS 20000U
#define SPI_MASTER_READ_FILL_BYTE 0xFFU

// ATmega328P hardware SPI master (mode 0, MSB first, selectable 1/8 MHz).
// One caller at a time. The UART interrupt does not touch this peripheral.
// Every byte transfer has a finite polling limit.
class SpiMaster {
public:
    SpiMaster() : timeouts_(0) {}
    enum class Speed : uint8_t { MHz1=0, MHz8=1 };
    void begin();
    void setSpeed(Speed speed);
    bool transfer(uint8_t outgoing, uint8_t& incoming);
    // One command and an optional data block under the caller's CSN.
    // The first received byte is the command's STATUS, and the remaining
    // bytes (if rx!=nullptr) are placed in rx. No per-byte function calls.
    // A timeout is enforced independently for each byte.
    bool transferBlock(uint8_t command,const uint8_t* tx,uint8_t* rx,
                       uint8_t count,uint8_t* statusByte);
    uint16_t timeouts() const { return timeouts_; }
private:
    uint16_t timeouts_;
};
#endif
