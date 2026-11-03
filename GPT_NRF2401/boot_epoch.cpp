#include "boot_epoch.h"
#include <avr/eeprom.h>
#include <stdint.h>

namespace {
    struct Record {
        uint32_t count;
        uint8_t check;
        uint8_t committed;
    };
    static const uint8_t RECORDS=16;
    static const uint8_t COMMIT=0xA5;
    Record EEMEM slots[RECORDS];

    uint8_t checksum(uint32_t v) {
        uint8_t crc=0x69;
        for (uint8_t i=0;i<4;++i) {
            crc^=static_cast<uint8_t>(v>>(8U*i));
            for (uint8_t j=0;j<8;++j) {
                crc=static_cast<uint8_t>(
                    (crc & 0x80U) ? (crc<<1)^0x07U : crc<<1);
            }
        }
        return crc;
    }
}

bool BootEpoch::advance(uint32_t& epoch) {
    bool found=false;
    uint32_t highest=0;
    uint8_t last=0;
    for (uint8_t i=0;i<RECORDS;++i) {
        if (eeprom_read_byte(&slots[i].committed)!=COMMIT) continue;
        const uint32_t value=eeprom_read_dword(&slots[i].count);
        const uint8_t check=eeprom_read_byte(&slots[i].check);
        if (check != checksum(value)) continue;
        if (!found || value>highest) {
            found=true;
            highest=value;
            last=i;
        }
    }
    // A terminal counter wrap requires explicit provisioning, not reuse
    // of the same nonce. No silent identity roll-over.
    if (found && highest==0xFFFFFFFFUL) return false;
    const uint8_t nextIndex=found ?
        static_cast<uint8_t>((last+1U)%RECORDS) : 0U;
    const uint32_t value=found ? highest+1UL : 1UL;
    // Commit marker is written last. A torn write is never a valid record.
    eeprom_update_byte(&slots[nextIndex].committed,0);
    eeprom_update_dword(&slots[nextIndex].count,value);
    eeprom_update_byte(&slots[nextIndex].check,checksum(value));
    eeprom_update_byte(&slots[nextIndex].committed,COMMIT);
    if (eeprom_read_byte(&slots[nextIndex].committed)!=COMMIT ||
        eeprom_read_dword(&slots[nextIndex].count)!=value ||
        eeprom_read_byte(&slots[nextIndex].check)!=checksum(value))
        return false;
    epoch=value;
    return true;
}
