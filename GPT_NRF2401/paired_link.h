#ifndef GPT_NRF24_PAIRED_LINK_H
#define GPT_NRF24_PAIRED_LINK_H
#include <stdint.h>
#include "retained_byte_ring.h"
#include "paired_frame.h"

// Shared two-peer RF link state. No UART registers, MCU types or heap.
// Stream DATA seq identifies logical 24B user segments; IDLE does not
// advance sequence. RX_LAST is cumulative acceptance ACK (set by stream).
// A received RF packet is never equated with application acceptance.
// PTX owns OBSERVE_TX.ARC_CNT; PRX echoes its last reported value.
class PairedLink {
  public:
	struct Counters {
		uint32_t rxFrames, gaps, duplicates, stale, foreign, rxDrops, txFrames;
		Counters();
	};
	enum class Receipt : uint8_t { NEW_FRAME, DUPLICATE, STALE, WRONG_SESSION };
	explicit PairedLink(bool isA, uint16_t sid);
	// Configure the same link-state object for either board role.
	// One instance per board suffices on the 2KB SRAM ATmega328P.
	void configure(bool isA, uint16_t sid);
	void resetSession(uint16_t sid);
	uint16_t sessionId() const;
	uint8_t peerCredit() const;
	uint8_t localLoss() const;
	uint8_t remoteLoss() const;
	uint8_t localDrops() const;
	const Counters& counters() const;
	void notePtxRetries(uint8_t arc);
	void noteLocalDrop();
	// Paired stream: MAC TX_SEQ is user DATA sequence and RX_LAST is
	// cumulative acceptance ACK. Both source types share the same header.
	PairedFrame::Header streamHeader(
		uint8_t seq, uint8_t ack, uint8_t credit, uint8_t length, bool missingPrefix) const;

	PairedFrame::Error makeStream(uint8_t seq, uint8_t ack, uint8_t credit, const uint8_t* data,
		uint8_t length, uint8_t* out, uint8_t& width, bool missingPrefix = false);

	PairedFrame::Error makeStreamFrom(uint8_t seq, uint8_t ack, uint8_t credit,
		const RetainedByteRing& source, uint8_t first, uint8_t length, uint8_t* out, uint8_t& width,
		bool missingPrefix = false);

	// Repeated DATA may contain a newer ACK and credit. All valid frames
	// reach the stream, which owns user-level deduplication.
	Receipt observeStream(const PairedFrame::View& f);

  private:
	void acceptStatus(const PairedFrame::View& f);
	bool isA_;
	uint16_t sid_;
	uint8_t peerLast_;
	bool peerSeen_;
	uint8_t localLoss_, remoteLoss_, localDrops_, arc_, peerCredit_;
	Counters counters_;
};
#endif
