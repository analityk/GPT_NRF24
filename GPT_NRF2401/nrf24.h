#ifndef GPT_NRF24_RADIO_H
#define GPT_NRF24_RADIO_H
#include "hardware_bindings.h"

#include <stdint.h>
#include "foreground_ports.h"

#define NRF24_RF_SETUP_1_MBPS 0x06U
#define NRF24_RF_SETUP_2_MBPS 0x0EU
#define NRF24_SPI_READ_FILL_BYTE 0xFFU

// Register and FIFO driver. Hardware bindings select concrete SPI/GPIO
// on AVR and explicit fake interfaces in host tests. No template dispatch.
// Timing and radio role transitions belong to NrfAckPayloadPhy.
class Nrf24 {
  public:
	enum Register : uint8_t {
		CONFIG = 0x00,
		EN_AA = 0x01,
		EN_RXADDR = 0x02,
		SETUP_AW = 0x03,
		SETUP_RETR = 0x04,
		RF_CH = 0x05,
		RF_SETUP = 0x06,
		STATUS = 0x07,
		OBSERVE_TX = 0x08,
		RX_ADDR_P0 = 0x0A,
		TX_ADDR = 0x10,
		RX_PW_P0 = 0x11,
		FIFO_STATUS = 0x17,
		DYNPD = 0x1C,
		FEATURE = 0x1D
	};
	enum class Result : uint8_t { OK = 0, SPI_ERROR = 1, NO_RADIO = 2, VERIFY_ERROR = 3 };

	struct Probe {
		Result result;
		uint8_t reg; // register at which failure occurred, if applicable
		uint8_t expected;
		uint8_t observed;
		uint8_t status;
		uint8_t channel;
		uint8_t config;
		uint8_t rfSetup;
		uint8_t fifoStatus;
		Probe();
	};

	Nrf24(SpiHandle spi, PinHandle ce, PinHandle csn);

	void begin();

	// Hardware CE belongs to this radio instance; PHY owns its transitions.
	void ceHigh();
	void ceLow();

	bool status(uint8_t& value);
	bool readRegister(uint8_t reg, uint8_t& value);
	bool writeRegister(uint8_t reg, uint8_t value);
	bool readRegisters(uint8_t reg, uint8_t* dst, uint8_t count);
	bool writeRegisters(uint8_t reg, const uint8_t* src, uint8_t count);
	bool flushRx();
	bool flushTx();
	bool clearIrq(uint8_t flags);
	// Enhanced ShockBurst ACK payload + dynamic payload support.
	// Some devices lock FEATURE until ACTIVATE 0x73; probe before toggling,
	// never blindly toggle a device where FEATURE already works.
	bool enableAckPayload(uint8_t retries = 0x23);

	bool readPayloadWidth(uint8_t& width);

	// Dynamic payload width must be read from the chip's RX FIFO. A corrupt
	// value is recovered by flushing RX, as required by the data sheet.
	bool readDynamicPayload(uint8_t* dst, uint8_t& width);

	// W_TX_PAYLOAD with a variable 1..32B payload. DPL must already be
	// enabled on the selected pipe. No fixed-width padding is transmitted.
	bool writeDynamicPayload(const uint8_t* bytes, uint8_t count);

	bool writeAckPayload(uint8_t pipe, const uint8_t* bytes, uint8_t count);

	// Safe bring-up: register read/write/restore test, then deterministic
	// RF settings, leaving PWR_UP=0 and CE LOW. No RF transmission occurs.
	Probe probeAndConfigure(uint8_t rfSetup = NRF24_RF_SETUP_1_MBPS);

  private:
	SpiHandle spi_;
	PinHandle ce_;
	PinHandle csn_;

	static Probe failure(Probe& p, Result result, uint8_t reg);

	// One CE-independent SPI transaction; releases CSN even on SPI failure.
	bool transaction(
		uint8_t command, const uint8_t* tx, uint8_t* rx, uint8_t count, uint8_t* statusByte);
};
#endif
