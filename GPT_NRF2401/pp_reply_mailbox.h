#ifndef GPT_NRF24_PP_REPLY_MAILBOX_H
#define GPT_NRF24_PP_REPLY_MAILBOX_H

#include <stdint.h>
#include "paired_frame.h"
#include "nrf24_ack_phy.h"

// One outstanding RX buffer, owned by the caller. Metadata is deferred
// until the NEXT radio exchange is on air. The caller MUST consume the
// mailbox before writing any new SPI reply into that RX buffer.
// No extra frame storage or heap is needed.
class PpReplyMailbox {
  public:
	PpReplyMailbox();

	bool pending() const;

	bool store(const NrfAckPayloadPhy::TxResult& outcome, uint8_t width);

	bool take(NrfAckPayloadPhy::TxResult& outcome, uint8_t& width);

	// A valid active DATA or IDLE cannot change the handshake epoch.
	// CONTROL, mismatched SID and malformed frames are processed before
	// launching the next TX: they may require a FIFO reset or new nonce.
	static bool canDefer(bool active, uint16_t sid, const uint8_t* frame, uint8_t width);

  private:
	bool pending_;
	uint8_t width_;
	NrfAckPayloadPhy::TxResult outcome_;
};

#endif
