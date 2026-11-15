#ifndef GPT_NRF24_PAIRED_SESSION_H
#define GPT_NRF24_PAIRED_SESSION_H
#include <stdint.h>
#include "paired_frame.h"

// Handshake above hardware ACK Payload. Initiator=A/PTX, responder=B/PRX.
// CONTROL always uses SID=0, opaque DATA uses negotiated SID16.
// Both peers identify their boots with a persistent 32-bit epoch and an
// in-boot 16-bit attempt counter. No AVR dependencies and no heap.
class PairedSession {
  public:
	struct Identity {
		uint32_t boot;
		uint16_t attempt;
		Identity(uint32_t b = 0, uint16_t a = 0);
		bool operator==(const Identity& v) const;
		bool operator!=(const Identity& v) const;
	};
	enum class Phase : uint8_t { DISCOVER, WAIT_READY, WAIT_READY_ACK, WAIT_CONFIRM, ACTIVE };
	enum class Op : uint8_t { HELLO = 1, BEACON = 2, WELCOME = 3, READY = 4, READY_ACK = 5 };
	enum class Event : uint8_t { NONE, CHANGED, ESTABLISHED, LOST };
	PairedSession(bool leader, Identity own);
	void configure(bool leader, Identity own);
	bool active() const;
	Phase phase() const;
	uint16_t sid() const;
	uint16_t generation() const;
	Identity own() const;
	Identity peer() const;

	// This may be called after a timeout or a restart beacon. Do not reuse
	// a control nonce from this boot; if attempts wrap the caller must first
	// acquire a NEW persistent epoch and call changeBoot().
	bool restart();
	void changeBoot(uint32_t epoch);
	// Queued ACK payloads are prepared before processing the triggering TX.
	// The caller must arrange for the prepared packet to be queued verbatim.
	bool prepare(uint8_t* out, uint8_t& width);

	Event accept(const PairedFrame::View& f);

	static uint16_t derive(Identity a, Identity b);

  private:
	static void put16(uint8_t* p, uint16_t v);
	static uint16_t get16(const uint8_t* p);
	static void putIdentity(uint8_t* p, Identity v);
	static Identity getIdentity(const uint8_t* p);
	bool leader_;
	Identity own_, peer_;
	uint16_t sid_;
	uint8_t seq_;
	Phase phase_;
	uint16_t generation_;
};
#endif
