#include "paired_frame.h"

PairedFrame::Header::Header()
	: txSeq(0),
	  rxLast(PAIRED_FRAME_INITIAL_ACK),
	  lossAB(0),
	  lossBA(0),
	  arcLast(0),
	  rxDrop(0),
	  rxCredit(0),
	  flags(0),
	  sessionId(0) {
}

PairedFrame::Kind PairedFrame::Header::kind() const {
	return static_cast<Kind>(flags & PAIRED_FRAME_KIND_MASK);
}

PairedFrame::View::View()
	: body(0),
	  length(0) {
}

PairedFrame::Error PairedFrame::validate(const Header& h, uint8_t length) {
	if (length > MAX_BODY) {
		return Error::BAD_LENGTH;
	}
	if ((h.lossAB | h.lossBA | h.arcLast | h.rxDrop) & PAIRED_FRAME_NIBBLE_OVERFLOW_MASK) {
		return Error::BAD_TYPE;
	}
	if (h.kind() == Kind::IDLE && length != 0) {
		return Error::BAD_TYPE;
	}
	if (h.kind() == Kind::CONTROL && length == 0) {
		return Error::BAD_TYPE;
	}
	return Error::OK;
}

void PairedFrame::encodeHeader(const Header& h, uint8_t* dst) {
	dst[PAIRED_FRAME_TX_SEQ_OFFSET] = h.txSeq;
	dst[PAIRED_FRAME_RX_LAST_OFFSET] = h.rxLast;
	dst[PAIRED_FRAME_LOSS_OFFSET] = (h.lossBA << PAIRED_FRAME_NIBBLE_SHIFT) | h.lossAB;
	dst[PAIRED_FRAME_DROP_RETRY_OFFSET] = (h.rxDrop << PAIRED_FRAME_NIBBLE_SHIFT) | h.arcLast;
	dst[PAIRED_FRAME_CREDIT_OFFSET] = h.rxCredit;
	dst[PAIRED_FRAME_FLAGS_OFFSET] = h.flags;
	dst[PAIRED_FRAME_SID_LOW_OFFSET] = h.sessionId;
	dst[PAIRED_FRAME_SID_HIGH_OFFSET] = h.sessionId >> PAIRED_FRAME_BYTE_BITS;
}

PairedFrame::Error PairedFrame::encode(
	const Header& h, const uint8_t* body, uint8_t length, uint8_t* dst, uint8_t& width) {
	if (!dst || (length != 0 && !body)) {
		return Error::NULL_BUFFER;
	}
	const Error error = validate(h, length);
	if (error != Error::OK) {
		return error;
	}
	encodeHeader(h, dst);
	for (uint8_t i = 0; i < length; ++i) {
		dst[HEADER_SIZE + i] = body[i];
	}
	width = HEADER_SIZE + length;
	return Error::OK;
}

PairedFrame::Error PairedFrame::encodeFrom(const Header& h, const RetainedByteRing& source,
	uint8_t first, uint8_t length, uint8_t* dst, uint8_t& width) {
	if (!dst) {
		return Error::NULL_BUFFER;
	}
	const Error error = validate(h, length);
	if (error != Error::OK) {
		return error;
	}
	encodeHeader(h, dst);
	for (uint8_t i = 0; i < length; ++i) {
		dst[HEADER_SIZE + i] = source.at(static_cast<uint8_t>(first + i));
	}
	width = HEADER_SIZE + length;
	return Error::OK;
}

PairedFrame::Error PairedFrame::decode(const uint8_t* src, uint8_t width, View& dst) {
	if (!src) {
		return Error::NULL_BUFFER;
	}
	if (width < HEADER_SIZE || width > MAX_FRAME) {
		return Error::BAD_LENGTH;
	}
	Header h;
	h.txSeq = src[PAIRED_FRAME_TX_SEQ_OFFSET];
	h.rxLast = src[PAIRED_FRAME_RX_LAST_OFFSET];
	h.lossAB = src[PAIRED_FRAME_LOSS_OFFSET] & PAIRED_FRAME_NIBBLE_MASK;
	h.lossBA = src[PAIRED_FRAME_LOSS_OFFSET] >> PAIRED_FRAME_NIBBLE_SHIFT;
	h.arcLast = src[PAIRED_FRAME_DROP_RETRY_OFFSET] & PAIRED_FRAME_NIBBLE_MASK;
	h.rxDrop = src[PAIRED_FRAME_DROP_RETRY_OFFSET] >> PAIRED_FRAME_NIBBLE_SHIFT;
	h.rxCredit = src[PAIRED_FRAME_CREDIT_OFFSET];
	h.flags = src[PAIRED_FRAME_FLAGS_OFFSET];
	h.sessionId =
		src[PAIRED_FRAME_SID_LOW_OFFSET] |
		(static_cast<uint16_t>(src[PAIRED_FRAME_SID_HIGH_OFFSET]) << PAIRED_FRAME_BYTE_BITS);
	const uint8_t length = width - HEADER_SIZE;
	const Error e = validate(h, length);
	if (e != Error::OK) {
		return e;
	}
	dst.header = h;
	dst.body = src + HEADER_SIZE;
	dst.length = length;
	return Error::OK;
}
