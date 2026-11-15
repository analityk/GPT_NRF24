#include "foreground_ports.h"

bool BytePort::read(uint8_t& value) const {
	if (!readFn) {
		return false;
	}
	return readFn(context, value);
}

uint8_t BytePort::available() const {
	if (!availableFn) {
		return 0;
	}
	return availableFn(context);
}

uint8_t BytePort::freeSpace() const {
	if (!freeSpaceFn) {
		return 0;
	}
	return freeSpaceFn(context);
}

bool BytePort::writeAll(const uint8_t* bytes, uint8_t count) const {
	if (!writeAllFn) {
		return false;
	}
	return writeAllFn(context, bytes, count);
}

RetainedByteRing& BytePort::retainedInput() const {
	return *retained;
}

uint32_t ClockPort::now() const {
	if (!nowFn) {
		return 0;
	}
	return nowFn(context);
}

uint16_t ClockPort::counter16() const {
	if (!counter16Fn) {
		return 0;
	}
	return counter16Fn(context);
}

void DelayPort::us(uint16_t count) const {
	if (usFn) {
		usFn(context, count);
	}
}

void DelayPort::ms(uint8_t count) const {
	if (msFn) {
		msFn(context, count);
	}
}

void PinPort::set() const {
	if (setFn) {
		setFn(context);
	}
}

void PinPort::clear() const {
	if (clearFn) {
		clearFn(context);
	}
}

void PinPort::outputHigh() const {
	if (outputHighFn) {
		outputHighFn(context);
	}
}

void PinPort::outputLow() const {
	if (outputLowFn) {
		outputLowFn(context);
	}
}

void PinPort::i2c_set() const {
	if (i2c_setFn) {
		i2c_setFn(context);
	}
}

uint8_t PinPort::read() const {
	if (!readFn) {
		return 0;
	}
	return readFn(context);
}

void SpiPort::begin() const {
	if (beginFn) {
		beginFn(context);
	}
}

bool SpiPort::transferBlock(
	uint8_t command, const uint8_t* tx, uint8_t* rx, uint8_t count, uint8_t* status) const {
	if (!transferBlockFn) {
		return false;
	}
	return transferBlockFn(context, command, tx, rx, count, status);
}

void RadioPort::ceLow() const {
	if (ceLowFn) {
		ceLowFn(context);
	}
}

void RadioPort::ceHigh() const {
	if (ceHighFn) {
		ceHighFn(context);
	}
}

bool RadioPort::writeRegister(uint8_t reg, uint8_t value) const {
	if (!writeRegisterFn) {
		return false;
	}
	return writeRegisterFn(context, reg, value);
}

bool RadioPort::readRegister(uint8_t reg, uint8_t& value) const {
	if (!readRegisterFn) {
		return false;
	}
	return readRegisterFn(context, reg, value);
}

bool RadioPort::clearIrq(uint8_t flags) const {
	if (!clearIrqFn) {
		return false;
	}
	return clearIrqFn(context, flags);
}

bool RadioPort::flushTx() const {
	if (!flushTxFn) {
		return false;
	}
	return flushTxFn(context);
}

bool RadioPort::flushRx() const {
	if (!flushRxFn) {
		return false;
	}
	return flushRxFn(context);
}

bool RadioPort::writeDynamicPayload(const uint8_t* data, uint8_t count) const {
	if (!writeDynamicPayloadFn) {
		return false;
	}
	return writeDynamicPayloadFn(context, data, count);
}

bool RadioPort::readDynamicPayload(uint8_t* data, uint8_t& count) const {
	if (!readDynamicPayloadFn) {
		return false;
	}
	return readDynamicPayloadFn(context, data, count);
}

bool RadioPort::writeAckPayload(uint8_t pipe, const uint8_t* data, uint8_t count) const {
	if (!writeAckPayloadFn) {
		return false;
	}
	return writeAckPayloadFn(context, pipe, data, count);
}

bool RadioPort::status(uint8_t& value) const {
	if (!statusFn) {
		return false;
	}
	return statusFn(context, value);
}

uint8_t StoragePort::read(uint8_t slot, uint8_t offset) const {
	if (!readFn) {
		return 0;
	}
	return readFn(context, slot, offset);
}

void StoragePort::update(uint8_t slot, uint8_t offset, uint8_t value) const {
	if (updateFn) {
		updateFn(context, slot, offset, value);
	}
}

void TracePort::sourceReady() const {
	if (sourceReadyFn) {
		sourceReadyFn(context);
	}
}

void TracePort::frameEncoded() const {
	if (frameEncodedFn) {
		frameEncodedFn(context);
	}
}
