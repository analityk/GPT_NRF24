#include "epoch_journal.h"

bool EpochJournal::advance(StoragePort store, uint32_t& value) {
	bool found = false;
	uint32_t highest = 0;
	uint8_t last = 0;
	for (uint8_t i = 0; i < EPOCH_JOURNAL_SLOTS; ++i) {
		if (store.read(i, EPOCH_JOURNAL_COMMIT_OFFSET) != EPOCH_JOURNAL_COMMIT_MARKER) {
			continue;
		}
		uint32_t count = 0;
		for (uint8_t k = 0; k < EPOCH_JOURNAL_COUNTER_BYTES; ++k) {
			count |= static_cast<uint32_t>(store.read(i, k)) << (EPOCH_JOURNAL_BYTE_BITS * k);
		}
		if (check(count) != store.read(i, EPOCH_JOURNAL_CHECK_OFFSET)) {
			continue;
		}
		if (!found || count > highest) {
			found = true;
			highest = count;
			last = i;
		}
	}
	if (found && highest == UINT32_MAX) {
		return false;
	}
	const uint8_t slot = found ? static_cast<uint8_t>((last + 1U) % EPOCH_JOURNAL_SLOTS) : 0;
	const uint32_t next = found ? highest + 1UL : 1UL;
	store.update(slot, EPOCH_JOURNAL_COMMIT_OFFSET, 0);
	for (uint8_t k = 0; k < EPOCH_JOURNAL_COUNTER_BYTES; ++k) {
		store.update(slot, k, static_cast<uint8_t>(next >> (EPOCH_JOURNAL_BYTE_BITS * k)));
	}
	store.update(slot, EPOCH_JOURNAL_CHECK_OFFSET, check(next));
	store.update(slot, EPOCH_JOURNAL_COMMIT_OFFSET, EPOCH_JOURNAL_COMMIT_MARKER);
	uint32_t checkValue = 0;
	for (uint8_t k = 0; k < EPOCH_JOURNAL_COUNTER_BYTES; ++k) {
		checkValue |= static_cast<uint32_t>(store.read(slot, k)) << (EPOCH_JOURNAL_BYTE_BITS * k);
	}
	if (store.read(slot, EPOCH_JOURNAL_COMMIT_OFFSET) != EPOCH_JOURNAL_COMMIT_MARKER ||
		store.read(slot, EPOCH_JOURNAL_CHECK_OFFSET) != check(next) || checkValue != next) {
		return false;
	}
	value = next;
	return true;
}

uint8_t EpochJournal::check(uint32_t value) {
	uint8_t crc = EPOCH_JOURNAL_CRC_INITIAL;
	for (uint8_t k = 0; k < EPOCH_JOURNAL_COUNTER_BYTES; ++k) {
		crc ^= value >> (EPOCH_JOURNAL_BYTE_BITS * k);
		for (uint8_t bit = 0; bit < EPOCH_JOURNAL_BYTE_BITS; ++bit) {
			crc = (crc & EPOCH_JOURNAL_CRC_HIGH_BIT) ? ((crc << 1) ^ EPOCH_JOURNAL_CRC_POLYNOMIAL)
													 : (crc << 1);
		}
	}
	return crc;
}
