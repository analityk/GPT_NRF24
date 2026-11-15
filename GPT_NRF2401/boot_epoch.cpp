#include "boot_epoch.h"
#include "epoch_journal.h"
#include <avr/eeprom.h>
#include <stdint.h>

namespace {
// 16 slots x 6 bytes: record 32-bit epoch, CRC8, commit byte.
// EEPROM contents are not initialized by flashing the firmware.
static uint8_t EEMEM journal[EPOCH_JOURNAL_SLOTS][EPOCH_JOURNAL_RECORD_BYTES];

struct EepromStorage {
  public:
	operator StoragePort() {
		StoragePort port = {};
		port.context = this;
		port.readFn = [](void* context, uint8_t slot, uint8_t offset) -> uint8_t {
			(void)context;
			return static_cast<EepromStorage*>(context)->read(slot, offset);
		};
		port.updateFn = [](void* context, uint8_t slot, uint8_t offset, uint8_t value) -> void {
			(void)context;
			static_cast<EepromStorage*>(context)->update(slot, offset, value);
		};
		return port;
	}

	uint8_t read(uint8_t slot, uint8_t offset) {
		return eeprom_read_byte(&journal[slot][offset]);
	}

	void update(uint8_t slot, uint8_t offset, uint8_t v) {
		eeprom_update_byte(&journal[slot][offset], v);
	}
};
} // namespace

bool BootEpoch::advance(uint32_t& out) {
	EepromStorage storage;
	return EpochJournal::advance(storage, out);
}
