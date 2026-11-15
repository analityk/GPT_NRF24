#ifndef GPT_NRF24_UART_RELIABLE_STREAM_H
#define GPT_NRF24_UART_RELIABLE_STREAM_H
#include "hardware_bindings.h"
#include <stdint.h>
#define UART_RELIABLE_MAX_NO_ACK_TURNS UINT8_MAX
#define UART_RELIABLE_TX_WINDOW_FRAMES 4U
#define UART_RELIABLE_REPLAY_AFTER_TURNS 8U
#include "paired_link.h"
#include "reliable_input_source.h"

// Full-rate bidirectional UART stream over opaque 24-byte RF BODY.
// DATA_SEQ and cumulative APP_ACK occupy MAC TX_SEQ/RX_LAST.
// All 24 BODY octets are user bytes, without transport preamble.
// Four unacknowledged segments per direction: new data can be emitted on
// successive RF exchanges while older segments await delayed ACK Payload.
// Go-back-N on ACK timeout (in EXCHANGES, not milliseconds), no heap.
// Received data is committed only in order, atomically by Port::writeAll.
// After a gap/timeout/rekey, drain the unacknowledged window first:
// IDLE still carries ACK/credit, but fresh DATA is not admitted until all
// previously cached segments are confirmed. No radio timing adjustment.
// All state is independent of AVR, UART registers, and radio implementation.
class UartReliableStream {
  public:
	static const uint8_t MAX_USER_BYTES = PairedFrame::MAX_BODY;
	static const uint8_t WINDOW = UART_RELIABLE_TX_WINDOW_FRAMES;
	static const uint8_t REPLAY_AFTER = UART_RELIABLE_REPLAY_AFTER_TURNS;

	explicit UartReliableStream(StreamHandle port);

	// A new RF session starts a new 8-bit stream numbering epoch.
	// Keep ALL unacknowledged segments in their original byte order;
	// a surviving sender can retransmit them across peer reset.
	void beginSession(uint16_t sid);
	void disconnect();
	bool hasPending() const;
	bool hasInput() const;
	bool pauseHostInput() const;
	uint8_t inputFreeSpace() const;
	uint8_t unsentInputBytes() const;
	bool hasWork() const;
	uint16_t acceptedBytes() const;
	uint16_t acknowledgedBytes() const;
	uint16_t duplicates() const;
	uint16_t blocked() const;
	uint16_t badFrames() const;
	uint16_t retransmits() const;
	uint8_t outstanding() const;

	// Advertise actual sink headroom in conservative 24B units.
	// Incoming segments are still checked by an atomic UART writeAll.
	uint8_t credit() const;

	// A frozen link still needs IDLE probes carrying the last accepted ACK,
	// but MUST NOT drain host input or replay stale DATA during RF silence.
	bool make(PairedLink& link, uint8_t* dst, uint8_t& width, bool permitData = true);

	// Optional portable observer for profiling source/cache work separately
	// from final MAC serialization. It neither reads a clock itself nor
	// changes stream state, so mock clocks can test both boundaries.
	bool makeTraced(
		PairedLink& link, uint8_t* dst, uint8_t& width, bool permitData, TracePort trace);

	// Return whether a nonempty user segment was received. This is NOT
	// proof of success: a full sink may refuse it and leave ACK unchanged.
	bool accept(const PairedFrame::View& f);

  private:
	// Descriptors follow the oldest unacknowledged DATA, not the
	// current 8-bit wire sequence. This preserves retained bytes on
	// session reset even when the old sendBase had wrapped or advanced.
	uint8_t slotIndex(uint8_t seq) const;
	void acceptAck(uint8_t ack);

	StreamHandle port_;
	ReliableInputSource source_;
	uint16_t session_;
	uint8_t baseSeq_, nextSeq_, headIndex_, recvNext_, outstanding_;
	uint8_t replayCount_, replaySeq_, noAckTurns_;
	bool gapPending_, nackSeen_;
	uint8_t nackForBase_;
	bool draining_;
	uint8_t slotStart_[WINDOW];
	uint8_t slotLength_[WINDOW];
	uint16_t acceptedBytes_, acknowledgedBytes_;
	uint16_t duplicates_, blocked_, badFrames_, retransmits_;
};
#endif
