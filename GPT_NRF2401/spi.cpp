#include "spi.h"
#include "board_clock.h"

#ifdef GPT_HOST_TEST
#include "../tests/spi_register_mock.h"
#else
#include <avr/io.h>
#endif

#define SPI_BYTE_TIMEOUT_POLLS 255U
#define SPI_READ_FILL_BYTE 0xFFU

#ifndef GPT_HOST_TEST
static inline bool spiMasterEnabled() {
	return (SPCR & _BV(MSTR)) != 0;
}

static inline void spiWriteByte(uint8_t value) {
	SPDR = value;
}

static inline bool spiByteReady() {
	return (SPSR & _BV(SPIF)) != 0;
}

static inline uint8_t spiReadByte() {
	return SPDR;
}
#endif

// Small local helper: no object or callback per byte on AVR.
static inline bool shiftByte(uint8_t outgoing, uint8_t& incoming) {
	spiWriteByte(outgoing);
	uint8_t remaining = SPI_BYTE_TIMEOUT_POLLS;
	do {
		if (spiByteReady()) {
			incoming = spiReadByte();
			return true;
		}
		--remaining;
	} while (remaining != 0);
	return false;
}

void SpiMaster::begin() {
#ifndef GPT_HOST_TEST
	// Hardware SS must remain an output, even though CSN uses another pin.
	DDRB |= _BV(DDB2) | _BV(DDB3) | _BV(DDB5);
	DDRB &= ~_BV(DDB4);
	SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPR0);
	SPSR = 0;
#endif
}

void SpiMaster::setSpeed(Speed speed) {
#ifndef GPT_HOST_TEST
	if (speed == Speed::MHz8) {
		SPCR = _BV(SPE) | _BV(MSTR);
		SPSR = _BV(SPI2X);
	} else {
		SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPR0);
		SPSR = 0;
	}
#else
	(void)speed;
#endif
}

bool SpiMaster::transfer(uint8_t outgoing, uint8_t& incoming) {
	if (!spiMasterEnabled() || !shiftByte(outgoing, incoming)) {
		++timeouts_;
		return false;
	}
	return true;
}

bool SpiMaster::transferBlock(
	uint8_t command, const uint8_t* tx, uint8_t* rx, uint8_t count, uint8_t* statusByte) {
	uint8_t received = 0;
	if (!transfer(command, received)) {
		return false;
	}
	if (statusByte) {
		*statusByte = received;
	}
	for (uint8_t i = 0; i < count; ++i) {
		uint8_t outgoing = SPI_READ_FILL_BYTE;
		if (tx) {
			outgoing = tx[i];
		}
		if (!shiftByte(outgoing, received)) {
			++timeouts_;
			return false;
		}
		if (rx) {
			rx[i] = received;
		}
	}
	return true;
}

SpiMaster::SpiMaster()
	: timeouts_(0) {
}

uint16_t SpiMaster::timeouts() const {
	return timeouts_;
}
