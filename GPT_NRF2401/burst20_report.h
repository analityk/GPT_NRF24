#ifndef GPT_NRF24_BURST20_REPORT_H
#define GPT_NRF24_BURST20_REPORT_H
#include <stdint.h>
#include "burst20_schedule.h"

// End-of-slot 32B control frame: 8B PairedFrame header + exactly 24B BODY.
// The codec is independent of SPI, ATmega EEPROM, clocks or UART.
// A received bitmap acknowledges a PREVIOUS block from the opposite owner,
// never the just-completed block that this sender has not yet heard about.
namespace Burst20Report {
static const uint8_t VERSION = 1;
static const uint8_t BODY_SIZE = 24;
static const uint8_t MAX_REPORTED_PACKETS = 32;

enum Flag : uint8_t {
	OVERFLOW = 0x01,
	RESYNC = 0x02,
	MISSED_REPORT = 0x04,
	PEER_PAUSED = 0x08,
	CRC_MISMATCH = 0x10
};

struct Summary {
	uint16_t cycle;
	uint16_t peerCycle;
	uint32_t dataCrc32;
	uint32_t peerBitmap;
	uint16_t dataBytes;
	uint8_t firstSeq;
	uint8_t dataPackets;
	uint8_t acceptedPeerPackets;
	uint8_t missingPeerPackets;
	uint8_t peerCredit;
	uint8_t flags;
	Burst20Schedule::Share share;
	Burst20Schedule::Owner owner;

	Summary();
};

void put16(uint8_t* dst, uint16_t value);
void put32(uint8_t* dst, uint32_t value);
uint16_t get16(const uint8_t* src);
uint32_t get32(const uint8_t* src);

bool valid(const Summary& v);

bool encode(const Summary& v, uint8_t* dst, uint8_t width);
bool decode(const uint8_t* src, uint8_t width, Summary& dst);

// CRC-32/ISO-HDLC: reflected poly 0xEDB88320, init and xorout 0xFFFFFFFF.
// Incremental state fits in 4B, so no 1024B block buffer on the MCU.
class RollingCrc32 {
  public:
	RollingCrc32();
	void reset();
	void add(uint8_t byte);
	void add(const uint8_t* data, uint8_t length);
	uint32_t value() const;

  private:
	uint32_t state_;
};

// A bounded receipt bitmap for a 32-slot block. Idempotent on duplicated
// frames: the reported count changes only on the first valid reception.
class Receipt {
  public:
	Receipt();
	void reset();
	bool accept(uint8_t index);
	uint32_t bitmap() const;
	uint8_t accepted() const;
	uint8_t missing(uint8_t expected) const;

  private:
	uint32_t bitmap_;
	uint8_t accepted_;
};
} // namespace Burst20Report
#endif
