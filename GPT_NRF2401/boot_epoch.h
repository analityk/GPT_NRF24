#ifndef GPT_NRF24_BOOT_EPOCH_H
#define GPT_NRF24_BOOT_EPOCH_H
#include <stdint.h>

// An EEPROM-backed persistent session discriminator, once per power-up.
// Another epoch is written only if the 16-bit session-attempt count expires.
class BootEpoch {
public:
    static bool advance(uint32_t& out);
};
#endif
