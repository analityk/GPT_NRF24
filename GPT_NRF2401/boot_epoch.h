#ifndef GPT_NRF24_BOOT_EPOCH_H
#define GPT_NRF24_BOOT_EPOCH_H
#include <stdint.h>

// Wear-levelled EEPROM monotonic boot epoch. One write per MCU startup,
// not per RF retry or discovery. Old committed records survive an interrupted
// write. The returned value is a session discriminator, not a secret.
class BootEpoch {
public:
    static bool advance(uint32_t& epoch);
};
#endif
