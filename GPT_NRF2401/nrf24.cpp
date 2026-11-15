
#define NRF24_REGISTER_READ_MAX 0x1FU
#define NRF24_REGISTER_WRITE_MAX 0x1DU
#define NRF24_RESERVED_REGISTER_FIRST 0x18U
#define NRF24_RESERVED_REGISTER_LAST 0x1BU
#define NRF24_COMMAND_WRITE_REGISTER 0x20U
#define NRF24_COMMAND_NOP 0xFFU
#define NRF24_COMMAND_FLUSH_RX 0xE2U
#define NRF24_COMMAND_FLUSH_TX 0xE1U
#define NRF24_COMMAND_ACTIVATE 0x50U
#define NRF24_ACTIVATE_FEATURES 0x73U
#define NRF24_COMMAND_READ_WIDTH 0x60U
#define NRF24_COMMAND_READ_PAYLOAD 0x61U
#define NRF24_COMMAND_WRITE_PAYLOAD 0xA0U
#define NRF24_COMMAND_WRITE_ACK 0xA8U
#define NRF24_IRQ_CLEAR_MASK 0x70U
#define NRF24_FEATURE_ACK_DPL 0x06U
#define NRF24_PIPE_ZERO_MASK 0x01U
#define NRF24_LAST_PIPE 5U
#define NRF24_ADDRESS_BYTES 5U
#define NRF24_PAYLOAD_BYTES 32U
#define NRF24_MAX_CHANNEL 125U
#define NRF24_CONFIG_POWER_DOWN_CRC16 0x0CU
#define NRF24_ADDRESS_WIDTH_FIVE 0x03U
#define NRF24_PROBE_RETRY_SETUP 0x3FU
#define NRF24_DEFAULT_CHANNEL 76U
#define NRF24_PROBE_PATTERN_FIRST 0x2AU
#define NRF24_PROBE_PATTERN_SECOND 0x55U
#define NRF24_PROBE_PATTERN_COUNT 2U
#define NRF24_ADDRESS_REGISTER_COUNT 2U

#ifndef GPT_HOST_TEST
#include "spi.h"
#include "gpio.h"
#endif
#include "nrf24.h"

Nrf24::Probe::Probe()
	: result(Result::SPI_ERROR),
	  reg(0),
	  expected(0),
	  observed(0),
	  status(0),
	  channel(0),
	  config(0),
	  rfSetup(0),
	  fifoStatus(0) {
}

Nrf24::Nrf24(SpiHandle spi, PinHandle ce, PinHandle csn)
	: spi_(spi),
	  ce_(ce),
	  csn_(csn) {
}

void Nrf24::begin() {
	// Prepare GPIO output direction once. Older Pin adapters still use
	// set()/clear(), without an extra requirement on portable mocks.
	ce_.outputLow();   // standby/power-down before SPI setup
	csn_.outputHigh(); // deselect chip
	spi_.begin();
}

void Nrf24::ceHigh() {
	ce_.set();
}

void Nrf24::ceLow() {
	ce_.clear();
}

bool Nrf24::status(uint8_t& value) {
	return transaction(NRF24_COMMAND_NOP, 0, 0, 0, &value);
}

bool Nrf24::readRegister(uint8_t reg, uint8_t& value) {
	if (reg > NRF24_REGISTER_READ_MAX) {
		return false;
	}
	return transaction(reg, 0, &value, 1, 0);
}

bool Nrf24::writeRegister(uint8_t reg, uint8_t value) {
	if (reg > NRF24_REGISTER_WRITE_MAX ||
		(reg >= NRF24_RESERVED_REGISTER_FIRST && reg <= NRF24_RESERVED_REGISTER_LAST)) {
		return false;
	}
	return transaction(static_cast<uint8_t>(NRF24_COMMAND_WRITE_REGISTER | reg), &value, 0, 1, 0);
}

bool Nrf24::readRegisters(uint8_t reg, uint8_t* dst, uint8_t count) {
	if (reg > NRF24_REGISTER_READ_MAX || !dst || count == 0 || count > NRF24_ADDRESS_BYTES) {
		return false;
	}
	return transaction(reg, 0, dst, count, 0);
}

bool Nrf24::writeRegisters(uint8_t reg, const uint8_t* src, uint8_t count) {
	if (reg > NRF24_REGISTER_WRITE_MAX ||
		(reg >= NRF24_RESERVED_REGISTER_FIRST && reg <= NRF24_RESERVED_REGISTER_LAST) || !src ||
		count == 0 || count > NRF24_ADDRESS_BYTES) {
		return false;
	}
	return transaction(static_cast<uint8_t>(NRF24_COMMAND_WRITE_REGISTER | reg), src, 0, count, 0);
}

bool Nrf24::flushRx() {
	return transaction(NRF24_COMMAND_FLUSH_RX, 0, 0, 0, 0);
}

bool Nrf24::flushTx() {
	return transaction(NRF24_COMMAND_FLUSH_TX, 0, 0, 0, 0);
}

bool Nrf24::clearIrq(uint8_t flags) {
	return writeRegister(STATUS, static_cast<uint8_t>(flags & NRF24_IRQ_CLEAR_MASK));
}

bool Nrf24::enableAckPayload(uint8_t retries) {
	uint8_t feature = 0;
	if (!writeRegister(FEATURE, NRF24_FEATURE_ACK_DPL) || !readRegister(FEATURE, feature)) {
		return false;
	}
	if (feature != NRF24_FEATURE_ACK_DPL) {
		const uint8_t activate = NRF24_ACTIVATE_FEATURES;
		if (!transaction(NRF24_COMMAND_ACTIVATE, &activate, 0, 1, 0) ||
			!writeRegister(FEATURE, NRF24_FEATURE_ACK_DPL) || !readRegister(FEATURE, feature) ||
			feature != NRF24_FEATURE_ACK_DPL) {
			return false;
		}
	}
	uint8_t dynamic = 0;
	uint8_t retryRead = 0;
	if (!writeRegister(DYNPD, NRF24_PIPE_ZERO_MASK) || !readRegister(DYNPD, dynamic) ||
		dynamic != NRF24_PIPE_ZERO_MASK || !writeRegister(SETUP_RETR, retries) ||
		!readRegister(SETUP_RETR, retryRead) || retryRead != retries) {
		return false;
	}
	return clearIrq(NRF24_IRQ_CLEAR_MASK) && flushRx() && flushTx();
}

bool Nrf24::readPayloadWidth(uint8_t& width) {
	return transaction(NRF24_COMMAND_READ_WIDTH, 0, &width, 1, 0);
}

bool Nrf24::readDynamicPayload(uint8_t* dst, uint8_t& width) {
	if (!dst || !readPayloadWidth(width)) {
		return false;
	}
	if (width == 0 || width > NRF24_PAYLOAD_BYTES) {
		(void)flushRx();
		return false;
	}
	return transaction(NRF24_COMMAND_READ_PAYLOAD, 0, dst, width, 0);
}

bool Nrf24::writeDynamicPayload(const uint8_t* bytes, uint8_t count) {
	return bytes && count >= 1 && count <= NRF24_PAYLOAD_BYTES &&
		   transaction(NRF24_COMMAND_WRITE_PAYLOAD, bytes, 0, count, 0);
}

bool Nrf24::writeAckPayload(uint8_t pipe, const uint8_t* bytes, uint8_t count) {
	if (pipe > NRF24_LAST_PIPE || !bytes || count == 0 || count > NRF24_PAYLOAD_BYTES) {
		return false;
	}
	return transaction(static_cast<uint8_t>(NRF24_COMMAND_WRITE_ACK | pipe), bytes, 0, count, 0);
}

Nrf24::Probe Nrf24::probeAndConfigure(uint8_t rfSetup) {
	Probe p;
	begin();
	if (!status(p.status)) {
		return failure(p, Result::SPI_ERROR, STATUS);
	}
	if (!readRegister(RF_CH, p.channel)) {
		return failure(p, Result::SPI_ERROR, RF_CH);
	}
	if (p.status == NRF24_COMMAND_NOP || p.channel > NRF24_MAX_CHANNEL) {
		return failure(p, Result::NO_RADIO, RF_CH);
	}

	const uint8_t oldChannel = p.channel;
	const uint8_t patterns[] = {NRF24_PROBE_PATTERN_FIRST, NRF24_PROBE_PATTERN_SECOND};
	for (uint8_t i = 0; i < NRF24_PROBE_PATTERN_COUNT; ++i) {
		if (!writeRegister(RF_CH, patterns[i])) {
			(void)writeRegister(RF_CH, oldChannel);
			return failure(p, Result::SPI_ERROR, RF_CH);
		}
		uint8_t received = 0;
		if (!readRegister(RF_CH, received)) {
			(void)writeRegister(RF_CH, oldChannel);
			return failure(p, Result::SPI_ERROR, RF_CH);
		}
		if (received != patterns[i]) {
			(void)writeRegister(RF_CH, oldChannel);
			p.expected = patterns[i];
			p.observed = received;
			return failure(p, Result::VERIFY_ERROR, RF_CH);
		}
	}
	if (!writeRegister(RF_CH, oldChannel)) {
		return failure(p, Result::SPI_ERROR, RF_CH);
	}

	// CRC16, radio powered down, initial width NRF24_PAYLOAD_BYTES B, 5 B addressing,
	// selectable 1/2 Mbps via explicit RF_SETUP value,
	// auto ACK on pipe0 only; chosen channel NRF24_DEFAULT_CHANNEL (2476 MHz).
	const uint8_t pairs[][2] = {{CONFIG, NRF24_CONFIG_POWER_DOWN_CRC16},
		{EN_AA, NRF24_PIPE_ZERO_MASK}, {EN_RXADDR, NRF24_PIPE_ZERO_MASK},
		{SETUP_AW, NRF24_ADDRESS_WIDTH_FIVE}, {SETUP_RETR, NRF24_PROBE_RETRY_SETUP},
		{RF_CH, NRF24_DEFAULT_CHANNEL}, {RF_SETUP, rfSetup}, {RX_PW_P0, NRF24_PAYLOAD_BYTES},
		{DYNPD, 0}, {FEATURE, 0}};
	const uint8_t pairCount = sizeof(pairs) / sizeof(pairs[0]);
	for (uint8_t i = 0; i < pairCount; ++i) {
		const uint8_t reg = pairs[i][0];
		const uint8_t expected = pairs[i][1];
		if (!writeRegister(reg, expected)) {
			return failure(p, Result::SPI_ERROR, reg);
		}
		uint8_t observed = 0;
		if (!readRegister(reg, observed)) {
			return failure(p, Result::SPI_ERROR, reg);
		}
		if (observed != expected) {
			p.expected = expected;
			p.observed = observed;
			return failure(p, Result::VERIFY_ERROR, reg);
		}
	}

	// Shared test radio address; session-specific routing is future work.
	const uint8_t address[NRF24_ADDRESS_BYTES] = {0xD2, 0xF0, 0xF0, 0xF0, 0xA1};
	const uint8_t addrRegs[NRF24_ADDRESS_REGISTER_COUNT] = {RX_ADDR_P0, TX_ADDR};
	for (uint8_t a = 0; a < NRF24_ADDRESS_REGISTER_COUNT; ++a) {
		const uint8_t reg = addrRegs[a];
		if (!writeRegisters(reg, address, NRF24_ADDRESS_BYTES)) {
			return failure(p, Result::SPI_ERROR, reg);
		}
		uint8_t readBack[NRF24_ADDRESS_BYTES] = {};
		if (!readRegisters(reg, readBack, NRF24_ADDRESS_BYTES)) {
			return failure(p, Result::SPI_ERROR, reg);
		}
		for (uint8_t i = 0; i < NRF24_ADDRESS_BYTES; ++i) {
			if (readBack[i] != address[i]) {
				p.reg = reg;
				p.expected = address[i];
				p.observed = readBack[i];
				return failure(p, Result::VERIFY_ERROR, reg);
			}
		}
	}

	if (!clearIrq(NRF24_IRQ_CLEAR_MASK) || !flushRx() || !flushTx()) {
		return failure(p, Result::SPI_ERROR, STATUS);
	}
	if (!status(p.status) || !readRegister(CONFIG, p.config) || !readRegister(RF_CH, p.channel) ||
		!readRegister(RF_SETUP, p.rfSetup) || !readRegister(FIFO_STATUS, p.fifoStatus)) {
		return failure(p, Result::SPI_ERROR, STATUS);
	}
	p.result = Result::OK;
	return p;
}

Nrf24::Probe Nrf24::failure(Probe& p, Result result, uint8_t reg) {
	p.result = result;
	p.reg = reg;
	return p;
}

bool Nrf24::transaction(
	uint8_t command, const uint8_t* tx, uint8_t* rx, uint8_t count, uint8_t* statusByte) {
	if (statusByte) {
		*statusByte = 0;
	}
	csn_.clear();
	const bool ok = spi_.transferBlock(command, tx, rx, count, statusByte);
	csn_.set();
	return ok;
}
