#include "boot_epoch.h"
#include "epoch_journal.h"
#include <avr/eeprom.h>
#include <stdint.h>

namespace {
    // 16 slots x 6 bytes: record 32-bit epoch, CRC8, commit byte.
    // EEPROM contents are not initialized by flashing the firmware.
    static uint8_t EEMEM journal[16][6];
    struct EepromStorage {
        uint8_t read(uint8_t slot,uint8_t offset) {
            return eeprom_read_byte(&journal[slot][offset]);
        }
        void update(uint8_t slot,uint8_t offset,uint8_t v) {
            eeprom_update_byte(&journal[slot][offset],v);
        }
    };
}
bool BootEpoch::advance(uint32_t& out) {
    EepromStorage storage;
    return EpochJournal<EepromStorage>::advance(storage,out);
}
