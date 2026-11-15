#include "paired_session.h"

PairedSession::Identity::Identity(uint32_t b, uint16_t a)
	: boot(b),
	  attempt(a) {
}

bool PairedSession::Identity::operator==(const Identity& v) const {
	return boot == v.boot && attempt == v.attempt;
}

bool PairedSession::Identity::operator!=(const Identity& v) const {
	return !(*this == v);
}

PairedSession::PairedSession(bool leader, Identity own)
	: leader_(leader),
	  own_(own),
	  peer_(),
	  sid_(0),
	  seq_(0),
	  phase_(Phase::DISCOVER),
	  generation_(0) {
}

void PairedSession::configure(bool leader, Identity own) {
	leader_ = leader;
	own_ = own;
	peer_ = Identity();
	sid_ = 0;
	seq_ = 0;
	phase_ = Phase::DISCOVER;
	generation_ = 0;
}

bool PairedSession::active() const {
	return phase_ == Phase::ACTIVE;
}

PairedSession::Phase PairedSession::phase() const {
	return phase_;
}

uint16_t PairedSession::sid() const {
	return sid_;
}

uint16_t PairedSession::generation() const {
	return generation_;
}

PairedSession::Identity PairedSession::own() const {
	return own_;
}

PairedSession::Identity PairedSession::peer() const {
	return peer_;
}

bool PairedSession::restart() {
	if (own_.attempt == 0xFFFFU) {
		return false;
	}
	++own_.attempt;
	peer_ = Identity();
	sid_ = 0;
	seq_ = 0;
	phase_ = Phase::DISCOVER;
	return true;
}

void PairedSession::changeBoot(uint32_t epoch) {
	own_ = Identity(epoch, 0);
	peer_ = Identity();
	sid_ = 0;
	seq_ = 0;
	phase_ = Phase::DISCOVER;
}

bool PairedSession::prepare(uint8_t* out, uint8_t& width) {
	if (active() || !out || own_.boot == 0) {
		return false;
	}
	const Op op = leader_ ? (phase_ == Phase::WAIT_READY_ACK ? Op::READY : Op::HELLO)
						  : (phase_ == Phase::WAIT_READY		? Op::WELCOME
								: phase_ == Phase::WAIT_CONFIRM ? Op::READY_ACK
																: Op::BEACON);
	uint8_t body[15] = {};
	body[0] = static_cast<uint8_t>(op);
	putIdentity(body + 1, own_);
	uint8_t length = 7;
	if (op == Op::WELCOME || op == Op::READY || op == Op::READY_ACK) {
		putIdentity(body + 7, peer_);
		length = 13;
	}
	if (op == Op::READY || op == Op::READY_ACK) {
		put16(body + 13, sid_);
		length = 15;
	}
	PairedFrame::Header h;
	h.sessionId = 0;
	h.txSeq = seq_;
	h.flags = static_cast<uint8_t>(PairedFrame::Kind::CONTROL) | PairedFrame::RESYNC;
	const PairedFrame::Error e = PairedFrame::encode(h, body, length, out, width);
	if (e != PairedFrame::Error::OK) {
		return false;
	}
	++seq_;
	return true;
}

PairedSession::Event PairedSession::accept(const PairedFrame::View& f) {
	// Active-state data from A proves that A received READY_ACK.
	// For B this is the only safe transition to application DATA.
	if (!leader_ && phase_ == Phase::WAIT_CONFIRM &&
		(f.header.kind() == PairedFrame::Kind::DATA ||
			f.header.kind() == PairedFrame::Kind::IDLE) &&
		sid_ != 0 && f.header.sessionId == sid_) {
		phase_ = Phase::ACTIVE;
		++generation_;
		return Event::ESTABLISHED;
	}
	if (f.header.kind() != PairedFrame::Kind::CONTROL || f.header.sessionId != 0 ||
		(f.header.flags & PairedFrame::RESYNC) == 0 || f.length < 7 || !f.body) {
		return Event::NONE;
	}
	const uint8_t op = f.body[0];
	const Identity sentBy = getIdentity(f.body + 1);
	if (sentBy.boot == 0) {
		return Event::NONE;
	}

	if (leader_) {
		// Beacon from a rebooted/reconfigured B tells an ACTIVE A to
		// stop transmitting the old session before the next DATA.
		if (op == static_cast<uint8_t>(Op::BEACON) && f.length == 7) {
			if (phase_ == Phase::ACTIVE && sentBy == peer_) {
				return Event::NONE;
			} // delayed pre-handshake ACK
			if (phase_ == Phase::DISCOVER) {
				return Event::NONE;
			}
			if (sentBy == peer_) {
				return Event::NONE;
			}
			if (!restart()) {
				return Event::LOST;
			} // caller renews EEPROM boot
			return Event::LOST;
		}
		if (op == static_cast<uint8_t>(Op::WELCOME) && f.length == 13 &&
			phase_ == Phase::DISCOVER && getIdentity(f.body + 7) == own_) {
			peer_ = sentBy;
			sid_ = derive(own_, peer_);
			phase_ = Phase::WAIT_READY_ACK;
			return Event::CHANGED;
		}
		if (op == static_cast<uint8_t>(Op::READY_ACK) && f.length == 15 &&
			phase_ == Phase::WAIT_READY_ACK && sentBy == peer_ && getIdentity(f.body + 7) == own_ &&
			get16(f.body + 13) == sid_) {
			phase_ = Phase::ACTIVE;
			++generation_;
			return Event::ESTABLISHED;
		}
	} else {
		if (op == static_cast<uint8_t>(Op::HELLO) && f.length == 7) {
			if (phase_ == Phase::ACTIVE && sentBy == peer_) {
				return Event::NONE;
			} // stale HELLO
			if ((phase_ == Phase::WAIT_READY || phase_ == Phase::WAIT_CONFIRM) && sentBy == peer_) {
				return Event::NONE;
			}
			const bool wasActive = (phase_ == Phase::ACTIVE);
			peer_ = sentBy;
			sid_ = derive(peer_, own_);
			phase_ = Phase::WAIT_READY;
			return wasActive ? Event::LOST : Event::CHANGED;
		}
		if (op == static_cast<uint8_t>(Op::READY) && f.length == 15 &&
			(phase_ == Phase::WAIT_READY || phase_ == Phase::WAIT_CONFIRM) && sentBy == peer_ &&
			getIdentity(f.body + 7) == own_ && get16(f.body + 13) == sid_) {
			if (phase_ != Phase::WAIT_CONFIRM) {
				phase_ = Phase::WAIT_CONFIRM;
				return Event::CHANGED;
			}
			return Event::NONE;
		}
	}
	return Event::NONE;
}

uint16_t PairedSession::derive(Identity a, Identity b) {
	// CRC-16/CCITT-FALSE; discriminator, NOT cryptographic identity.
	uint8_t bytes[14] = {0x50, 0x32};
	putIdentity(bytes + 2, a);
	putIdentity(bytes + 8, b);
	uint16_t crc = 0xFFFFU;
	for (uint8_t i = 0; i < 14; ++i) {
		crc ^= static_cast<uint16_t>(bytes[i]) << 8;
		for (uint8_t k = 0; k < 8; ++k) {
			crc = (crc & 0x8000U) ? ((crc << 1) ^ 0x1021U) : (crc << 1);
		}
	}
	return crc ? crc : 1;
}

void PairedSession::put16(uint8_t* p, uint16_t v) {
	p[0] = v;
	p[1] = v >> 8;
}

uint16_t PairedSession::get16(const uint8_t* p) {
	return static_cast<uint16_t>(p[0] | static_cast<uint16_t>(p[1]) << 8);
}

void PairedSession::putIdentity(uint8_t* p, Identity v) {
	for (uint8_t i = 0; i < 4; ++i) {
		p[i] = v.boot >> (8U * i);
	}
	put16(p + 4, v.attempt);
}

PairedSession::Identity PairedSession::getIdentity(const uint8_t* p) {
	uint32_t boot = 0;
	for (uint8_t i = 0; i < 4; ++i) {
		boot |= static_cast<uint32_t>(p[i]) << (8U * i);
	}
	return Identity(boot, get16(p + 4));
}
