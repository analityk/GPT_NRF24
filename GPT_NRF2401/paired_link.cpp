#include "paired_link.h"
#define PAIRED_SEQUENCE_FORWARD_LIMIT (UINT8_MAX / 2U)

PairedLink::Counters::Counters()
	: rxFrames(0),
	  gaps(0),
	  duplicates(0),
	  stale(0),
	  foreign(0),
	  rxDrops(0),
	  txFrames(0) {
}

PairedLink::PairedLink(bool isA, uint16_t sid)
	: isA_(isA),
	  sid_(sid),
	  peerLast_(0),
	  peerSeen_(false),
	  localLoss_(0),
	  remoteLoss_(0),
	  localDrops_(0),
	  arc_(0),
	  peerCredit_(0) {
}

void PairedLink::configure(bool isA, uint16_t sid) {
	isA_ = isA;
	resetSession(sid);
}

void PairedLink::resetSession(uint16_t sid) {
	sid_ = sid;
	peerLast_ = 0;
	peerSeen_ = false;
	localLoss_ = 0;
	remoteLoss_ = 0;
	localDrops_ = 0;
	arc_ = 0;
	peerCredit_ = 0;
	counters_ = Counters();
}

uint16_t PairedLink::sessionId() const {
	return sid_;
}

uint8_t PairedLink::peerCredit() const {
	return peerCredit_;
}

uint8_t PairedLink::localLoss() const {
	return localLoss_;
}

uint8_t PairedLink::remoteLoss() const {
	return remoteLoss_;
}

uint8_t PairedLink::localDrops() const {
	return localDrops_;
}

const PairedLink::Counters& PairedLink::counters() const {
	return counters_;
}

void PairedLink::notePtxRetries(uint8_t arc) {
	if (isA_) {
		arc_ = arc > PAIRED_FRAME_NIBBLE_MASK ? PAIRED_FRAME_NIBBLE_MASK : arc;
	}
}

void PairedLink::noteLocalDrop() {
	localDrops_ = (localDrops_ + 1U) & PAIRED_FRAME_NIBBLE_MASK;
	++counters_.rxDrops;
}

PairedFrame::Header PairedLink::streamHeader(
	uint8_t seq, uint8_t ack, uint8_t credit, uint8_t length, bool missingPrefix) const {
	PairedFrame::Header h;
	h.txSeq = seq;
	h.rxLast = ack;
	h.lossAB = isA_ ? remoteLoss_ : localLoss_;
	h.lossBA = isA_ ? localLoss_ : remoteLoss_;
	h.arcLast = arc_;
	h.rxDrop = localDrops_;
	h.rxCredit = credit;
	h.flags = static_cast<uint8_t>(length != 0 ? PairedFrame::Kind::DATA : PairedFrame::Kind::IDLE);
	if (missingPrefix) {
		h.flags = h.flags | PairedFrame::URGENT;
	}
	h.sessionId = sid_;
	return h;
}

PairedFrame::Error PairedLink::makeStream(uint8_t seq, uint8_t ack, uint8_t credit,
	const uint8_t* data, uint8_t length, uint8_t* out, uint8_t& width, bool missingPrefix) {
	const PairedFrame::Header h = streamHeader(seq, ack, credit, length, missingPrefix);
	const PairedFrame::Error error = PairedFrame::encode(h, data, length, out, width);
	if (error == PairedFrame::Error::OK) {
		++counters_.txFrames;
	}
	return error;
}

PairedFrame::Error PairedLink::makeStreamFrom(uint8_t seq, uint8_t ack, uint8_t credit,
	const RetainedByteRing& source, uint8_t first, uint8_t length, uint8_t* out, uint8_t& width,
	bool missingPrefix) {
	const PairedFrame::Header h = streamHeader(seq, ack, credit, length, missingPrefix);
	const PairedFrame::Error error = PairedFrame::encodeFrom(h, source, first, length, out, width);
	if (error == PairedFrame::Error::OK) {
		++counters_.txFrames;
	}
	return error;
}

PairedLink::Receipt PairedLink::observeStream(const PairedFrame::View& f) {
	if (f.header.sessionId != sid_) {
		++counters_.foreign;
		return Receipt::WRONG_SESSION;
	}
	++counters_.rxFrames;
	acceptStatus(f);
	if (f.header.kind() != PairedFrame::Kind::DATA || !f.length) {
		return Receipt::NEW_FRAME;
	}
	if (!peerSeen_) {
		peerLast_ = f.header.txSeq;
		peerSeen_ = true;
		return Receipt::NEW_FRAME;
	}
	const uint8_t delta = f.header.txSeq - peerLast_;
	if (delta == 0) {
		++counters_.duplicates;
		return Receipt::DUPLICATE;
	}
	if (delta > PAIRED_SEQUENCE_FORWARD_LIMIT) {
		++counters_.stale;
		return Receipt::STALE;
	}
	if (delta > 1U) {
		const uint8_t missing = delta - 1U;
		counters_.gaps += missing;
		localLoss_ = (localLoss_ + missing) & PAIRED_FRAME_NIBBLE_MASK;
	}
	peerLast_ = f.header.txSeq;
	return Receipt::NEW_FRAME;
}

void PairedLink::acceptStatus(const PairedFrame::View& f) {
	// B owns A->B loss, A owns B->A loss; peer field is delayed echo.
	remoteLoss_ = isA_ ? f.header.lossAB : f.header.lossBA;
	peerCredit_ = f.header.rxCredit;
	if (!isA_) {
		arc_ = f.header.arcLast;
	}
}
