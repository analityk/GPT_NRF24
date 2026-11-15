#include "pp_reply_mailbox.h"

PpReplyMailbox::PpReplyMailbox()
	: pending_(false),
	  width_(0),
	  outcome_() {
}

bool PpReplyMailbox::pending() const {
	return pending_;
}

bool PpReplyMailbox::store(const NrfAckPayloadPhy::TxResult& outcome, uint8_t width) {
	if (pending_) {
		return false;
	}
	outcome_ = outcome;
	width_ = width;
	pending_ = true;
	return true;
}

bool PpReplyMailbox::take(NrfAckPayloadPhy::TxResult& outcome, uint8_t& width) {
	if (!pending_) {
		return false;
	}
	outcome = outcome_;
	width = width_;
	pending_ = false;
	return true;
}

bool PpReplyMailbox::canDefer(bool active, uint16_t sid, const uint8_t* frame, uint8_t width) {
	if (!active) {
		return false;
	}
	PairedFrame::View view;
	if (PairedFrame::decode(frame, width, view) != PairedFrame::Error::OK) {
		return false;
	}
	const PairedFrame::Kind kind = view.header.kind();
	return view.header.sessionId == sid &&
		   (kind == PairedFrame::Kind::DATA || kind == PairedFrame::Kind::IDLE);
}
