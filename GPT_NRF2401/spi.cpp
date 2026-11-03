#include "spi.h"
#include "board_clock.h"
#include <avr/io.h>

void SpiMaster::begin() {
    // PB2 is the hardware SS pin: MUST remain configured as OUTPUT,
    // even though on legacy RF-Nano PB2 is wired to radio CE (not CSN).
    // CE must be set LOW by the radio driver before calling begin().
    DDRB |= _BV(DDB2) | _BV(DDB3) | _BV(DDB5); // SS, MOSI, SCK outputs
    DDRB &= static_cast<uint8_t>(~_BV(DDB4)); // MISO input
    SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPR0);   // SPI mode 0, F_CPU / 16
    SPSR = 0;                                  // SPI2X = 0
}

// Set speed only when no SPI transaction is active. Preserve master mode 0.
void SpiMaster::setSpeed(Speed speed) {
    if (speed==Speed::MHz8) {
        SPCR = _BV(SPE) | _BV(MSTR); // F_CPU/4
        SPSR = _BV(SPI2X);          // double speed = F_CPU/2
    } else {
        SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPR0); // F_CPU/16
        SPSR = 0;
    }
}

bool SpiMaster::transfer(uint8_t outgoing, uint8_t& incoming) {
    if (!(SPCR & _BV(MSTR))) {
        ++timeouts_;
        return false;
    }
    SPDR = outgoing;
    uint16_t remaining = 20000U;
    while (!(SPSR & _BV(SPIF))) {
        if (--remaining == 0U) {
            ++timeouts_;
            return false;
        }
    }
    incoming = SPDR; // SPIF cleared by reading SPSR then SPDR
    return true;
}
