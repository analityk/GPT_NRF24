#ifndef GPT_FOREGROUND_PORTS_H
#define GPT_FOREGROUND_PORTS_H
#include <stdint.h>
#include "retained_byte_ring.h"

// Explicit platform boundaries, used only by foreground code.
// Callbacks never run in an interrupt. All storage belongs to the caller.
struct BytePort {
	void* context;
	RetainedByteRing* retained;
	bool sharedInput;
	bool (*readFn)(void*, uint8_t& value);
	uint8_t (*availableFn)(void*);
	uint8_t (*freeSpaceFn)(void*);
	bool (*writeAllFn)(void*, const uint8_t* bytes, uint8_t count);
	bool read(uint8_t& value) const;
	uint8_t available() const;
	uint8_t freeSpace() const;
	bool writeAll(const uint8_t* bytes, uint8_t count) const;
	RetainedByteRing& retainedInput() const;
};

struct ClockPort {
	void* context;
	uint32_t (*nowFn)(void*);
	uint16_t (*counter16Fn)(void*);
	uint32_t now() const;
	uint16_t counter16() const;
};

struct DelayPort {
	void* context;
	void (*usFn)(void*, uint16_t count);
	void (*msFn)(void*, uint8_t count);
	void us(uint16_t count) const;
	void ms(uint8_t count) const;
};

struct PinPort {
	void* context;
	void (*setFn)(void*);
	void (*clearFn)(void*);
	void (*outputHighFn)(void*);
	void (*outputLowFn)(void*);
	void (*i2c_setFn)(void*);
	uint8_t (*readFn)(void*);
	void set() const;
	void clear() const;
	void outputHigh() const;
	void outputLow() const;
	void i2c_set() const;
	uint8_t read() const;
};

struct SpiPort {
	void* context;
	void (*beginFn)(void*);
	bool (*transferBlockFn)(
		void*, uint8_t command, const uint8_t* tx, uint8_t* rx, uint8_t count, uint8_t* status);
	void begin() const;
	bool transferBlock(
		uint8_t command, const uint8_t* tx, uint8_t* rx, uint8_t count, uint8_t* status) const;
};

struct RadioPort {
	void* context;

	enum Register { CONFIG = 0x00, OBSERVE_TX = 0x08, FIFO_STATUS = 0x17 };

	void (*ceLowFn)(void*);
	void (*ceHighFn)(void*);
	bool (*writeRegisterFn)(void*, uint8_t reg, uint8_t value);
	bool (*readRegisterFn)(void*, uint8_t reg, uint8_t& value);
	bool (*clearIrqFn)(void*, uint8_t flags);
	bool (*flushTxFn)(void*);
	bool (*flushRxFn)(void*);
	bool (*writeDynamicPayloadFn)(void*, const uint8_t* data, uint8_t count);
	bool (*readDynamicPayloadFn)(void*, uint8_t* data, uint8_t& count);
	bool (*writeAckPayloadFn)(void*, uint8_t pipe, const uint8_t* data, uint8_t count);
	bool (*statusFn)(void*, uint8_t& value);
	void ceLow() const;
	void ceHigh() const;
	bool writeRegister(uint8_t reg, uint8_t value) const;
	bool readRegister(uint8_t reg, uint8_t& value) const;
	bool clearIrq(uint8_t flags) const;
	bool flushTx() const;
	bool flushRx() const;
	bool writeDynamicPayload(const uint8_t* data, uint8_t count) const;
	bool readDynamicPayload(uint8_t* data, uint8_t& count) const;
	bool writeAckPayload(uint8_t pipe, const uint8_t* data, uint8_t count) const;
	bool status(uint8_t& value) const;
};

struct StoragePort {
	void* context;
	uint8_t (*readFn)(void*, uint8_t slot, uint8_t offset);
	void (*updateFn)(void*, uint8_t slot, uint8_t offset, uint8_t value);
	uint8_t read(uint8_t slot, uint8_t offset) const;
	void update(uint8_t slot, uint8_t offset, uint8_t value) const;
};

struct TracePort {
	void* context;
	void (*sourceReadyFn)(void*);
	void (*frameEncodedFn)(void*);
	void sourceReady() const;
	void frameEncoded() const;
};
#endif
