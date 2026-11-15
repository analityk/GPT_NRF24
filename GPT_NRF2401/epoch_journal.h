#ifndef GPT_NRF24_EPOCH_JOURNAL_H
#define GPT_NRF24_EPOCH_JOURNAL_H
#include <stdint.h>
#include "foreground_ports.h"
#define EPOCH_JOURNAL_SLOTS 16U
#define EPOCH_JOURNAL_RECORD_BYTES 6U
#define EPOCH_JOURNAL_COUNTER_BYTES 4U
#define EPOCH_JOURNAL_CHECK_OFFSET 4U
#define EPOCH_JOURNAL_COMMIT_OFFSET 5U
#define EPOCH_JOURNAL_COMMIT_MARKER 0xA5U
#define EPOCH_JOURNAL_CRC_INITIAL 0x69U
#define EPOCH_JOURNAL_CRC_POLYNOMIAL 0x07U
#define EPOCH_JOURNAL_CRC_HIGH_BIT 0x80U
#define EPOCH_JOURNAL_BYTE_BITS 8U

// Wear-levelled monotonically increasing boot epoch. Storage adapter offers
// uint8_t read(slot,offset), void update(slot,offset,value).
// Commit is written last, leaving the previous record valid on torn writes.
// No heap and no assumptions about AVR EEPROM representation.
class EpochJournal {

  public:
	static bool advance(StoragePort store, uint32_t& value);

  private:
	static uint8_t check(uint32_t value);
};
#endif
