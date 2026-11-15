#include "pp_application_internal.h"

namespace PpApp {
void fatal() {
	// This temporary firmware owns the diagnostic UART, not a binary
	// payload stream. Make a halted test distinguishable from RF silence.
#if PP_TEST_TRAFFIC_MODE == PP_TRAFFIC_MODE_SYNTHETIC_RADIO
	const uint8_t marker[] = {'F', 'A', 'T', 'A', 'L', '\r', '\n'};
	(void)uart.writeAll(marker, sizeof(marker));
#endif
	for (;;) {
		delay.ms(100);
	}
}

static bool restartIdentity() {
	if (!session.restart()) {
		uint32_t next = 0;
		if (!BootEpoch::advance(next)) {
			return false;
		}
		session.changeBoot(next);
	}
	linkState.configure(isLeader, 0);
	stream.disconnect(); // Retains the pending, unacknowledged UART segment.
	++recoveryCount;
	lastPeerProgress = ticks.now();
	return true;
}

static bool makePacketTraced(uint8_t* dst, uint8_t& width, TracePort trace) {
	if (!session.active()) {
		return session.prepare(dst, width);
	}
	if (linkState.sessionId() != session.sid()) {
		linkState.configure(isLeader, session.sid());
		stream.beginSession(session.sid());
	}
	return stream.makeTraced(linkState, dst, width, recovery.canSendData(), trace);
}

static bool makePacket(uint8_t* dst, uint8_t& width) {
	TracePort trace = {};
	return makePacketTraced(dst, width, trace);
}

// Handshake must be decoded BEFORE the strict active-session filter.
// Hardware ACK is never interpreted here as acceptance of UART bytes.
static void processFrame(uint8_t width) {
	PairedFrame::View view;
	if (PairedFrame::decode(incoming, width, view) != PairedFrame::Error::OK) {
		++radioErrors;
		return;
	}
	const PairedSession::Event event = session.accept(view);
	if (event != PairedSession::Event::NONE) {
		const uint32_t now = ticks.now();
		lastPeerProgress = now;
		if (event == PairedSession::Event::LOST) {
			recovery.peerStateLost(now, stream.hasPending());
			if (isLeader && session.active()) {
				uint32_t epoch = 0;
				if (!BootEpoch::advance(epoch)) {
					fatal();
				}
				session.changeBoot(epoch);
			}
			linkState.configure(isLeader, 0);
			stream.disconnect();
			++recoveryCount;
			if (isLeader && !phy.resetPtxFifos()) {
				++radioErrors;
			}
		} else if (event == PairedSession::Event::ESTABLISHED) {
			linkState.configure(isLeader, session.sid());
			stream.beginSession(session.sid());
			recovery.established(now);
		} else if (!session.active()) {
			linkState.configure(isLeader, 0);
			stream.disconnect();
		}
		// PRX must invalidate any preloaded ACK from a prior phase/epoch.
		if (!isLeader) {
			replaceAck = true;
		}
	}
	if (view.header.kind() == PairedFrame::Kind::CONTROL) {
		return;
	}
	if (!session.active() || view.header.sessionId != session.sid()) {
		return;
	}
	const PairedLink::Receipt receipt = linkState.observeStream(view);
	if (receipt == PairedLink::Receipt::WRONG_SESSION) {
		return;
	}
	const uint32_t now = ticks.now();
	lastPeerProgress = now;
	recovery.validPeerFrame(now);
	// A repeated DATA sequence may carry a NEW application ACK.
	(void)stream.accept(view);
}

// Priority 2: bounded application/diagnostic work while the nRF itself
// performs the physical DATA/ACK exchange. Protocol state is not modified
// by an unprocessed ACK: the previous reply is always consumed before the
// next stream.make(), protecting credit, sequence and rekey ordering.
static void serviceAInFlight() {
	const uint32_t now = ticks.now();
	recovery.poll(now);
#if PP_TEST_TRAFFIC_MODE == PP_TRAFFIC_MODE_SYNTHETIC_RADIO
	diagnosticPort.service(now, recovery.canProduce(), PP_PTX_BACKGROUND_MAX_GENERATED_BYTES);
#endif
#if PP_TEST_TRAFFIC_MODE == PP_TRAFFIC_MODE_SYNTHETIC_RADIO
	reportPerformance();
	reportRecoveryTransition();
#endif
	if (PP_PTX_TIMING_PROBE_ENABLED) {
		reportCycleTiming();
	}
	if (PP_SOURCE_PROBE_ENABLED) {
		reportSourceProbe(now);
	}
#if PP_UART_DIAG_ENABLED
	serviceUartDiagnostic();
#endif
}

// The PHY completed the hardware exchange. The MAC completion can be
// delayed one turn, but hardware retry accounting cannot be skipped.
static void processAReply(const AckPhy::TxResult& tx, uint8_t width) {
	if (tx.code == AckPhy::TxCode::ACK_WITH_DATA) {
		processFrame(width);
	} else if (tx.code == AckPhy::TxCode::BAD_REPLY_SIZE || tx.code == AckPhy::TxCode::IO_ERROR ||
			   tx.code == AckPhy::TxCode::TIMEOUT) {
		++radioErrors;
	}
}

void runA() {
	if (!phy.beginPtx()) {
		fatal();
	}
	delay.ms(PP_PTX_PEER_STARTUP_MS);
	PpReplyMailbox deferredReply;
	bool servicedInFlight = false;
	uint8_t inFlightWidth = 0;
	uint16_t prepareTicks = 0;
	uint16_t backgroundTicks = 0;
	uint16_t previousProtocolTicks = 0;
	uint16_t protocolOverlapTicks = 0;
	uint16_t sourceTicks = 0;
	uint16_t encodeTicks = 0;
	for (;;) {
		if (phy.txBusy()) {
			// The previous SPI RX buffer must be consumed BEFORE another
			// pollTransfer could overwrite it with the current ACK payload.
			// Starting the new RF turn first moves this work under ON_AIR.
			if (phy.txOnAir() && !servicedInFlight) {
				if (deferredReply.pending()) {
					AckPhy::TxResult previous;
					uint8_t previousWidth = 0;
					if (!deferredReply.take(previous, previousWidth)) {
						fatal();
					}
					const uint32_t protocolStart = PP_PTX_TIMING_PROBE_ENABLED ? ticks.now() : 0;
					processAReply(previous, previousWidth);
					if (PP_PTX_TIMING_PROBE_ENABLED) {
						protocolOverlapTicks = PpCycleTimingWindow::clampTicks(
							static_cast<uint32_t>(ticks.now() - protocolStart));
					}
				}
				const uint32_t workStart = PP_PTX_TIMING_PROBE_ENABLED ? ticks.now() : 0;
				serviceAInFlight();
				if (PP_PTX_TIMING_PROBE_ENABLED) {
					backgroundTicks = PpCycleTimingWindow::clampTicks(
						static_cast<uint32_t>(ticks.now() - workStart));
				}
				servicedInFlight = true;
			}
			uint8_t incomingWidth = 0;
			AckPhy::TxResult tx;
			const AckPhy::TxStep step = phy.pollTransfer(incoming, incomingWidth, tx);
			if (step == AckPhy::TxStep::PENDING) {
				continue;
			}
			if (step != AckPhy::TxStep::COMPLETE || deferredReply.pending()) {
				fatal();
			}
			const bool wasActive = session.active();
			const uint16_t oldSid = session.sid();
			linkState.notePtxRetries(tx.retries);
			hardwareRetries += tx.retries;
			if (tx.code == AckPhy::TxCode::MAX_RT) {
				++maxRetries;
			}
			// A control/foreign/invalid reply can trigger a session reset
			// and must be processed before any later payload is queued.
#if PP_UART_DIAG_ENABLED
			previousRfDataExchange = wasActive && session.active() && oldSid == session.sid() &&
									 recovery.state() == PpRecoveryControl::State::HEALTHY &&
									 tx.code == AckPhy::TxCode::ACK_WITH_DATA && tx.retries == 0 &&
									 (inFlightWidth > PairedFrame::HEADER_SIZE ||
										 incomingWidth > PairedFrame::HEADER_SIZE);
#endif
			const bool defer =
				tx.code == AckPhy::TxCode::ACK_WITH_DATA &&
				PpReplyMailbox::canDefer(session.active(), session.sid(), incoming, incomingWidth);
			uint16_t immediateProtocolTicks = 0;
			if (defer) {
				if (!deferredReply.store(tx, incomingWidth)) {
					fatal();
				}
			} else {
				const uint32_t protocolStart = PP_PTX_TIMING_PROBE_ENABLED ? ticks.now() : 0;
				processAReply(tx, incomingWidth);
				if (PP_PTX_TIMING_PROBE_ENABLED) {
					immediateProtocolTicks = PpCycleTimingWindow::clampTicks(
						static_cast<uint32_t>(ticks.now() - protocolStart));
				}
			}
			if (PP_PTX_TIMING_PROBE_ENABLED) {
				const bool cleanFullExchange =
					wasActive && session.active() && oldSid == session.sid() &&
					recovery.state() == PpRecoveryControl::State::HEALTHY &&
					tx.code == AckPhy::TxCode::ACK_WITH_DATA && tx.retries == 0 &&
					inFlightWidth == PairedFrame::MAX_FRAME &&
					incomingWidth == PairedFrame::MAX_FRAME;
				const uint16_t protocolTicks = PpCycleTimingWindow::clampTicks(
					static_cast<uint32_t>(previousProtocolTicks) + protocolOverlapTicks);
				cycleTiming.observe(phy.lastCompletedAt(), phy.lastTiming(), prepareTicks,
					protocolTicks, backgroundTicks, sourceTicks, encodeTicks, protocolOverlapTicks,
					cleanFullExchange);
				previousProtocolTicks = immediateProtocolTicks;
			}
			protocolOverlapTicks = 0;
			if (!servicedInFlight) {
				serviceAInFlight();
			}
			servicedInFlight = false;
			backgroundTicks = 0;
			if (!session.active() && session.phase() != PairedSession::Phase::DISCOVER &&
				static_cast<uint32_t>(ticks.now() - lastPeerProgress) >
					PP_PTX_HANDSHAKE_TIMEOUT_TICKS) {
				if (!restartIdentity() || !phy.resetPtxFifos()) {
					fatal();
				}
			}
			continue;
		}
		const uint32_t now = ticks.now();
		recovery.poll(now);
		if (session.active() && recovery.warmExpired(now)) {
			recovery.abandonWarm(now, stream.hasPending());
			if (!restartIdentity() || !phy.resetPtxFifos()) {
				fatal();
			}
		}
		if (session.active() && recovery.state() == PpRecoveryControl::State::OFFLINE &&
			static_cast<uint32_t>(now - lastProbe) < PP_OFFLINE_PROBE_INTERVAL_TICKS) {
			serviceAInFlight();
			delay.us(PP_OFFLINE_IDLE_US);
			continue;
		}
		lastProbe = now;
		uint8_t width = 0;
		const uint32_t prepareStart = PP_PTX_TIMING_PROBE_ENABLED ? ticks.now() : 0;
#if PP_SOURCE_PROBE_ENABLED
		PpShortSourceTrace makeTrace(ticks);
#elif PP_PTX_TIMING_PROBE_ENABLED
		PpFrameMakeTrace makeTrace(ticks, prepareStart);
#else
		TracePort makeTrace = {};
#endif
		if (!makePacketTraced(packet, width, makeTrace)) {
			fatal();
		}
#if PP_SOURCE_PROBE_ENABLED
		if (session.active() && width == PairedFrame::MAX_FRAME &&
			recovery.state() == PpRecoveryControl::State::HEALTHY && makeTrace.sampled()) {
			sourceProbe.observe(makeTrace.sourceTicks());
		}
#endif
		if (PP_PTX_TIMING_PROBE_ENABLED) {
			prepareTicks =
				PpCycleTimingWindow::clampTicks(static_cast<uint32_t>(ticks.now() - prepareStart));
		}
#if PP_SOURCE_PROBE_ENABLED || PP_PTX_TIMING_PROBE_ENABLED
		sourceTicks = makeTrace.sourceTicks();
		encodeTicks = makeTrace.encodeTicks();
#endif
		inFlightWidth = width;
#if PP_UART_DIAG_ENABLED
		const uint16_t rfCycleStart = ticks.counter16();
#endif
		if (!phy.startTransfer(packet, width)) {
#if PP_UART_DIAG_ENABLED
			markRfCycle(rfCycleStart, false);
#endif
			++radioErrors;
			if (PP_PTX_TIMING_PROBE_ENABLED) {
				cycleTiming.clearWindow();
			}
			if (!phy.resetPtxFifos()) {
				fatal();
			}
			// The deferred ACK is still in incoming and must be consumed
			// before reusing the buffer on the next successful transfer.
		}
#if PP_UART_DIAG_ENABLED
		else {
			markRfCycle(rfCycleStart, true);
		}
#endif
	}
}

static bool queueB() {
	if (replaceAck) {
		ackWaiting = false;
		if (!makePacket(ackStaged, ackSize)) {
			return false;
		}
		if (!phy.replacePrxReply(ackStaged, ackSize)) {
			return false;
		}
		replaceAck = false;
		return true;
	}
	if (!ackWaiting) {
		if (!makePacket(ackStaged, ackSize)) {
			return false;
		}
		ackWaiting = true;
	}
	const AckPhy::QueueCode q = phy.queueReply(ackStaged, ackSize);
	if (q == AckPhy::QueueCode::QUEUED) {
		ackWaiting = false;
		return true;
	}
	if (q == AckPhy::QueueCode::FULL) {
		++ackFifoFull;
	}
	if (q == AckPhy::QueueCode::IO_ERROR) {
		++radioErrors;
	}
	// FULL is normal when the PRX has a pipelined response outstanding.
	return false;
}

void runB() {
	if (!phy.beginPrx()) {
		fatal();
	}
	if (!queueB()) {
		fatal();
	} // initial BEACON for A's first HELLO
	for (;;) {
		const uint32_t now = ticks.now();
		recovery.poll(now);
		if (session.active() && recovery.warmExpired(now)) {
			recovery.abandonWarm(now, stream.hasPending());
			if (!restartIdentity()) {
				fatal();
			}
			replaceAck = true;
		}
		uint8_t incomingWidth = 0;
		const AckPhy::RxCode rx = phy.receive(incoming, incomingWidth);
		if (rx == AckPhy::RxCode::GOOD) {
			processFrame(incomingWidth);
			if (!phy.clearPrxFlags()) {
				++radioErrors;
			}
			(void)queueB();
		} else if (rx == AckPhy::RxCode::BAD_SIZE || rx == AckPhy::RxCode::IO_ERROR) {
			++radioErrors;
			if (!phy.clearPrxFlags()) {
				++radioErrors;
			}
		}
		if (!session.active() && session.phase() != PairedSession::Phase::DISCOVER &&
			static_cast<uint32_t>(ticks.now() - lastPeerProgress) >
				PP_PRX_HANDSHAKE_TIMEOUT_TICKS) {
			if (!restartIdentity()) {
				fatal();
			}
			replaceAck = true;
		}
		if (replaceAck && !queueB()) {
			++radioErrors;
		}
#if PP_TEST_TRAFFIC_MODE == PP_TRAFFIC_MODE_SYNTHETIC_RADIO
		reportPerformance();
		reportRecoveryTransition();
#endif
#if PP_UART_DIAG_ENABLED
		serviceUartDiagnostic();
#endif
	}
}

} // namespace PpApp
