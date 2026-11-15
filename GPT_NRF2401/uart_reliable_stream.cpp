#ifndef GPT_HOST_TEST
#include "pp_uart_bridge_port.h"
#include "pp_echo_ramp_port.h"
#endif
#include "uart_reliable_stream.h"

UartReliableStream::UartReliableStream(StreamHandle port)
	: port_(port),
	  source_(port),
	  session_(0),
	  baseSeq_(0),
	  nextSeq_(0),
	  headIndex_(0),
	  recvNext_(0),
	  outstanding_(0),
	  replayCount_(0),
	  replaySeq_(0),
	  noAckTurns_(0),
	  gapPending_(false),
	  nackSeen_(false),
	  nackForBase_(0),
	  draining_(false),
	  acceptedBytes_(0),
	  acknowledgedBytes_(0),
	  duplicates_(0),
	  blocked_(0),
	  badFrames_(0),
	  retransmits_(0) {
	for (uint8_t i = 0; i < WINDOW; ++i) {
		slotLength_[i] = 0;
		slotStart_[i] = 0;
	}
}

void UartReliableStream::beginSession(uint16_t sid) {
	if (session_ == sid) {
		return;
	}
	session_ = sid;
	baseSeq_ = 0;
	nextSeq_ = outstanding_;
	recvNext_ = 0;
	noAckTurns_ = 0;
	gapPending_ = false;
	nackSeen_ = false;
	replayCount_ = outstanding_;
	replaySeq_ = 0;
	draining_ = (outstanding_ != 0);
	// Physical data already accepted in an old epoch may be delivered
	// again if its final ACK was lost. Exactly-once across reset needs
	// a separate durable protocol, not this volatile UART transport.
}

void UartReliableStream::disconnect() {
	beginSession(0);
}

bool UartReliableStream::hasPending() const {
	return outstanding_ != 0;
}

bool UartReliableStream::hasInput() const {
	return source_.hasInput();
}

bool UartReliableStream::pauseHostInput() const {
	return source_.ring().pauseRequested();
}

uint8_t UartReliableStream::inputFreeSpace() const {
	return source_.ring().freeSpace();
}

uint8_t UartReliableStream::unsentInputBytes() const {
	return source_.ring().availableToPrepare();
}

bool UartReliableStream::hasWork() const {
	return hasPending() || hasInput();
}

uint16_t UartReliableStream::acceptedBytes() const {
	return acceptedBytes_;
}

uint16_t UartReliableStream::acknowledgedBytes() const {
	return acknowledgedBytes_;
}

uint16_t UartReliableStream::duplicates() const {
	return duplicates_;
}

uint16_t UartReliableStream::blocked() const {
	return blocked_;
}

uint16_t UartReliableStream::badFrames() const {
	return badFrames_;
}

uint16_t UartReliableStream::retransmits() const {
	return retransmits_;
}

uint8_t UartReliableStream::outstanding() const {
	return outstanding_;
}

uint8_t UartReliableStream::credit() const {
	return static_cast<uint8_t>(port_.freeSpace() / PairedFrame::MAX_BODY);
}

bool UartReliableStream::make(PairedLink& link, uint8_t* dst, uint8_t& width, bool permitData) {
	TracePort trace = {};
	return makeTraced(link, dst, width, permitData, trace);
}

bool UartReliableStream::makeTraced(
	PairedLink& link, uint8_t* dst, uint8_t& width, bool permitData, TracePort trace) {
	if (session_ == 0 || link.sessionId() != session_) {
		return false;
	}
	source_.pump();
	RetainedByteRing& ring = source_.ring();
	uint8_t seq = nextSeq_;
	uint8_t count = 0;
	uint8_t first = ring.prepared();
	uint8_t slot = 0;
	bool replay = false;
	bool fresh = false;
	const uint8_t peerCredit = link.peerCredit();
	const bool maySend = permitData && peerCredit != 0;

	if (maySend && outstanding_ != 0 && noAckTurns_ < UART_RELIABLE_MAX_NO_ACK_TURNS) {
		++noAckTurns_;
	}
	if (maySend && outstanding_ != 0 && replayCount_ == 0 && noAckTurns_ >= REPLAY_AFTER) {
		replaySeq_ = baseSeq_;
		replayCount_ = outstanding_;
		noAckTurns_ = 0;
		draining_ = true;
	}

	if (maySend && replayCount_ != 0) {
		seq = replaySeq_;
		slot = slotIndex(seq);
		first = slotStart_[slot];
		count = slotLength_[slot];
		replay = true;
	} else if (maySend && !draining_ && outstanding_ < WINDOW && outstanding_ < peerCredit) {
		count = ring.availableToPrepare();
		if (count > MAX_USER_BYTES) {
			count = MAX_USER_BYTES;
		}
		if (count != 0) {
			seq = nextSeq_;
			slot = slotIndex(seq);
			first = ring.prepared();
			fresh = true;
		}
	}

	trace.sourceReady();
	const PairedFrame::Error encoded =
		link.makeStreamFrom(seq, static_cast<uint8_t>(recvNext_ - 1U), credit(), ring, first, count,
			dst, width, gapPending_);
	trace.frameEncoded();
	if (encoded != PairedFrame::Error::OK) {
		return false;
	}
	if (replay) {
		++replaySeq_;
		--replayCount_;
		++retransmits_;
	} else if (fresh) {
		slotStart_[slot] = first;
		slotLength_[slot] = count;
		ring.markPrepared(count);
		++nextSeq_;
		++outstanding_;
	}
	return true;
}

bool UartReliableStream::accept(const PairedFrame::View& f) {
	if (session_ == 0 || f.header.sessionId != session_ ||
		(f.header.kind() != PairedFrame::Kind::DATA &&
			f.header.kind() != PairedFrame::Kind::IDLE)) {
		return false;
	}
	// Every frame, including IDLE and retransmitted DATA, conveys ACK.
	acceptAck(f.header.rxLast);
	// Go-back-N receiver reports a hole via an already allocated MAC
	// flag. Rewind immediately only ONCE per missing base sequence:
	// queued old ACK payloads otherwise cause a replay storm.
	if ((f.header.flags & PairedFrame::URGENT) != 0 && outstanding_ != 0 &&
		static_cast<uint8_t>(f.header.rxLast + 1U) == baseSeq_ &&
		(!nackSeen_ || nackForBase_ != baseSeq_)) {
		replaySeq_ = baseSeq_;
		replayCount_ = outstanding_;
		noAckTurns_ = 0;
		nackForBase_ = baseSeq_;
		nackSeen_ = true;
		draining_ = true;
	}
	if (f.header.kind() == PairedFrame::Kind::IDLE) {
		return false;
	}
	if (!f.length || !f.body) {
		++badFrames_;
		return false;
	}
	const uint8_t n = f.length;
	const uint8_t seq = f.header.txSeq;
	if (seq == recvNext_) {
		if (port_.writeAll(f.body, n)) {
			++recvNext_;
			gapPending_ = false;
			acceptedBytes_ = acceptedBytes_ + n;
		} else {
			++blocked_; // NEVER ACK undelivered user bytes
		}
	} else if (static_cast<uint8_t>(recvNext_ - seq) <= WINDOW) {
		// A duplicate inside our bounded sender window.
		++duplicates_;
	} else {
		// Out-of-order future frame: Go-back-N receiver. Sender must
		// replay the missing oldest segment before advancement.
		++badFrames_;
		gapPending_ = true;
	}
	return true;
}

uint8_t UartReliableStream::slotIndex(uint8_t seq) const {
	const uint8_t offset = seq - baseSeq_;
	return static_cast<uint8_t>((headIndex_ + offset) % WINDOW);
}

void UartReliableStream::acceptAck(uint8_t ack) {
	if (outstanding_ == 0) {
		return;
	}
	// Acknowledge at most the outstanding window, not a stale epoch.
	const uint8_t distance = ack - baseSeq_;
	if (distance >= outstanding_) {
		return;
	}
	const uint8_t n = distance + 1U;
	for (uint8_t i = 0; i < n; ++i) {
		const uint8_t slot = slotIndex(static_cast<uint8_t>(baseSeq_ + i));
		acknowledgedBytes_ = acknowledgedBytes_ + slotLength_[slot];
		// No memset: releasing the tail makes the space reusable by the ISR.
		source_.ring().acknowledge(slotLength_[slot]);
	}
	headIndex_ = (headIndex_ + n) % WINDOW;
	baseSeq_ = baseSeq_ + n;
	outstanding_ = outstanding_ - n;
	if (outstanding_ == 0) {
		draining_ = false;
	}
	noAckTurns_ = 0;
	nackSeen_ = false;
	// ACK progress cannot skip the unsent tail of a replay sequence.
	if (replayCount_ != 0) {
		const uint8_t delta = baseSeq_ - replaySeq_;
		if (delta <= WINDOW) {
			const uint8_t covered = delta < replayCount_ ? delta : replayCount_;
			replaySeq_ = replaySeq_ + covered;
			replayCount_ = replayCount_ - covered;
		}
	}
	if (replayCount_ == 0) {
		replaySeq_ = baseSeq_;
	}
}
