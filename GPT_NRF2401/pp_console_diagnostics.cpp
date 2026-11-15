#include "pp_application_internal.h"

namespace PpApp {
#if PP_UART_DIAG_ENABLED
static PpUartQuietDiagnostic uartQuietDiagnostic;
static PpUartDiagReport queuedDiagnostic;
static uint32_t uartDiagnosticPollAt = 0;
static uint16_t diagnosticTxBytes = 0;
static PpShortProbeWindow rfCycleProbe;
static uint16_t previousRfStart = 0;
static bool previousRfStartValid = false;
bool previousRfDataExchange = false;

enum class UartDiagnosticPhase : uint8_t { OBSERVE, UART_COUNTS, PROTOCOL_COUNTS, RF_CYCLE, TRACE };
static UartDiagnosticPhase uartDiagnosticPhase = UartDiagnosticPhase::OBSERVE;
static uint8_t uartDiagnosticTraceIndex = 0;

// One timing region: PTX start-to-start after a successful radio exchange
// carrying DATA in at least one direction (variable payload lengths).
void markRfCycle(uint16_t started, bool successful) {
	if (!successful) {
		previousRfStartValid = false;
		previousRfDataExchange = false;
		return;
	}
	if (previousRfStartValid && previousRfDataExchange) {
		rfCycleProbe.observe(static_cast<uint16_t>(started - previousRfStart));
	}
	previousRfStart = started;
	previousRfStartValid = true;
	previousRfDataExchange = false;
}

static PpUartDiagSnapshot uartDiagnosticSnapshot() {
	const UART::Counters counters = uart.counters();
	PpUartDiagSnapshot snapshot;
	snapshot.uartRx = counters.rx_bytes;
	// Subtract ASCII diagnostics from the physical UART TX count.
	snapshot.uartTx = counters.tx_bytes - diagnosticTxBytes;
	snapshot.rxFull = counters.rx_buffer_full;
	snapshot.rxOverrun = counters.rx_overrun;
	snapshot.rxFraming = counters.rx_framing;
	snapshot.acknowledged = stream.acknowledgedBytes();
	snapshot.accepted = stream.acceptedBytes();
	snapshot.retries = stream.retransmits();
	snapshot.blocked = stream.blocked();
	snapshot.bad = stream.badFrames();
	snapshot.hwArc = hardwareRetries;
	snapshot.maxRt = maxRetries;
	snapshot.ackFifoFull = ackFifoFull;
	snapshot.rxFifoFull = phy.rxFifoFullSamples();
	snapshot.sid = session.sid();
	snapshot.outstanding = stream.outstanding();
	snapshot.credit = linkState.peerCredit();
	snapshot.freeInput = stream.inputFreeSpace();
	return snapshot;
}

// Only the post-burst quiet interval allows a diagnostic ASCII report.
// UART input and RF output never carry any additional in-band commands.
void serviceUartDiagnostic() {
	if (uartDiagnosticPhase == UartDiagnosticPhase::OBSERVE) {
		const uint32_t now = ticks.now();
		if (static_cast<uint32_t>(now - uartDiagnosticPollAt) < PP_UART_DIAG_POLL_TICKS) {
			return;
		}
		uartDiagnosticPollAt = now;
		if (!uartQuietDiagnostic.observe(now, uartDiagnosticSnapshot(), queuedDiagnostic)) {
			return;
		}
		uartDiagnosticPhase = UartDiagnosticPhase::UART_COUNTS;
	}
	if (uart.freeSpace() < PP_UART_DIAG_LINE_FREE_BYTES) {
		return;
	}
	PpStressLine line;
	if (uartDiagnosticPhase == UartDiagnosticPhase::UART_COUNTS) {
		line.word(isLeader ? "A uart in=" : "B uart in=");
		line.decimal(queuedDiagnostic.uartRx);
		line.word(" out=");
		line.decimal(queuedDiagnostic.uartTx);
		line.word(" full=");
		line.decimal(queuedDiagnostic.rxFull);
		line.word(" dor=");
		line.decimal(queuedDiagnostic.rxOverrun);
		line.word(" fe=");
		line.decimal(queuedDiagnostic.rxFraming);
		line.word(" free=");
		line.decimal(queuedDiagnostic.freeInput);
		line.word("\r\n");
	} else if (uartDiagnosticPhase == UartDiagnosticPhase::PROTOCOL_COUNTS) {
		line.word(isLeader ? "A pp ack=" : "B pp ack=");
		line.decimal(queuedDiagnostic.acknowledged);
		line.word(" rx=");
		line.decimal(queuedDiagnostic.accepted);
		line.word(" ret=");
		line.decimal(queuedDiagnostic.retries);
		line.word(" blk=");
		line.decimal(queuedDiagnostic.blocked);
		line.word(" bad=");
		line.decimal(queuedDiagnostic.bad);
		line.word(" win=");
		line.decimal(queuedDiagnostic.outstanding);
		line.word(" cr=");
		line.decimal(queuedDiagnostic.credit);
		if (isLeader) {
			line.word(" hw=");
			line.decimal(queuedDiagnostic.hwArc);
			line.word(" mx=");
			line.decimal(queuedDiagnostic.maxRt);
		} else {
			line.word(" af=");
			line.decimal(queuedDiagnostic.ackFifoFull);
			line.word(" rf=");
			line.decimal(queuedDiagnostic.rxFifoFull);
		}
		line.word(" sid=");
		line.decimal(queuedDiagnostic.sid);
		line.word("\r\n");
	} else if (uartDiagnosticPhase == UartDiagnosticPhase::RF_CYCLE) {
		PpShortProbeWindow::Snapshot cycle;
		if (rfCycleProbe.sample(cycle)) {
			line.word("A rf n=");
			line.decimal(cycle.count);
			line.word(" cyc=");
			line.decimal(cycle.meanUs);
			line.word(" lo=");
			line.decimal(cycle.minUs);
			line.word(" hi=");
			line.decimal(cycle.maxUs);
			line.word("\r\n");
		} else {
			line.word("A rf n=0 (no valid DATA exchange sample)\r\n");
		}
	} else {
		while (!formatPpUartDiagTrace(
			line, isLeader, queuedDiagnostic.trace, uartDiagnosticTraceIndex)) {
			++uartDiagnosticTraceIndex;
			if (uartDiagnosticTraceIndex > PP_UART_DIAG_EVENT_COUNT) {
				uartDiagnosticPhase = UartDiagnosticPhase::OBSERVE;
				return;
			}
		}
	}
	if (!line.emit(uart)) {
		return;
	}
	diagnosticTxBytes = diagnosticTxBytes + line.length();
	if (uartDiagnosticPhase == UartDiagnosticPhase::UART_COUNTS) {
		uartDiagnosticPhase = UartDiagnosticPhase::PROTOCOL_COUNTS;
	} else if (uartDiagnosticPhase == UartDiagnosticPhase::PROTOCOL_COUNTS && isLeader) {
		uartDiagnosticPhase = UartDiagnosticPhase::RF_CYCLE;
	} else if (uartDiagnosticPhase != UartDiagnosticPhase::TRACE) {
		uartDiagnosticPhase = UartDiagnosticPhase::TRACE;
		uartDiagnosticTraceIndex = 0;
		rfCycleProbe.clear();
	} else {
		++uartDiagnosticTraceIndex;
		if (uartDiagnosticTraceIndex > PP_UART_DIAG_EVENT_COUNT) {
			uartDiagnosticPhase = UartDiagnosticPhase::OBSERVE;
		}
	}
}
#else
void serviceUartDiagnostic() {
}
#endif

#if PP_TEST_TRAFFIC_MODE == PP_TRAFFIC_MODE_SYNTHETIC_RADIO
void reportRecoveryTransition() {
	const PpRecoveryControl::State state = recovery.state();
	const bool uncertain = recovery.uncertainty();
	if (state == lastPrintedState && uncertain == lastPrintedUncertainty) {
		return;
	}
	const uint32_t now = ticks.now();
	// Do not let rapid HEALTHY/SUSPECT changes saturate the console or
	// compete with RF throughput. Sample the latest state at most 5 Hz.
	if (static_cast<uint32_t>(now - lastRecoveryReport) < 400000UL) {
		return;
	}
	lastRecoveryReport = now; // also throttle attempts if UART ring is full
	const HostFlowStatus::Pair status = HostFlowStatus::snapshot(
		recovery, diagnosticPort.inputFreeSpace(), diagnosticPort.freeSpace());
	PpStressLine line;
	line.word(isLeader ? "A link=" : "B link=");
	line.decimal(static_cast<uint8_t>(state));
	line.word(" tx=");
	line.decimal(status.tx);
	line.word(" rx=");
	line.decimal(status.rx);
	line.word(" warm=");
	line.decimal(recovery.warmRecoveries());
	line.word(" cold=");
	line.decimal(recovery.coldRecoveries());
	line.word(" exp=");
	line.decimal(recovery.expiredRecoveries());
	line.word(" uncertain=");
	line.decimal(uncertain ? 1U : 0U);
	line.word("\r\n");
	if (line.emit(uart)) {
		lastPrintedState = state;
		lastPrintedUncertainty = uncertain;
	}
}

PpStressSnapshot performanceSnapshot() {
	PpStressSnapshot s;
	s.sourceBytes = isLeader ? diagnosticPort.generated() : diagnosticPort.forwarded();
	s.deliveredBytes = isLeader ? diagnosticPort.echoed() : diagnosticPort.received();
	s.hardwareRetries = hardwareRetries;
	s.maxRetries = maxRetries;
	s.echoGaps = diagnosticPort.echoGaps();
	s.appRetries = stream.retransmits();
	s.duplicates = stream.duplicates();
	s.badFrames = stream.badFrames();
	s.blocked = stream.blocked();
	s.radioErrors = radioErrors;
	s.recoveries = recoveryCount;
	s.acknowledgedBytes = stream.acknowledgedBytes();
	s.outstanding = stream.outstanding();
	s.linkGaps = linkState.counters().gaps;
	s.linkStale = linkState.counters().stale;
	s.rxFifoFull = phy.rxFifoFullSamples();
	s.ackFifoFull = ackFifoFull;
	s.sid = session.sid();
	s.active = session.active();
	return s;
}

void reportPerformance() {
	const uint32_t now = ticks.now();
	if (static_cast<uint32_t>(now - lastReport) < 2000000UL) {
		return;
	}
	lastReport = now;
	const PpStressSnapshot snapshot = performanceSnapshot();
	PpStressSample sample;
	if (performance.take(now, snapshot, sample)) {
		(void)printPpStressLine(uart, isLeader, sample);
	}
}

#else
void reportRecoveryTransition() {
}

void reportPerformance() {
}
#endif

// Separate, short diagnostic line: per-exchange averages in microseconds.
// CPU time is the exclusive remainder; bg and the deferred part of pr
// overlap rf (they must not be added to the total exchange period).
// ACK time includes SPI reading the payload and clearing the IRQ.
void reportCycleTiming() {
#if PP_PTX_TIMING_PROBE_ENABLED

	// The UART ring cannot fit both lines at once. Queue the second line
	// only when transmission of the first has freed enough space; the
	// saved sample still refers to the SAME set of RF exchanges.
	if (timingDetailPending) {
		if (uart.freeSpace() < PP_TIMING_REPORT_MIN_FREE_BYTES) {
			return;
		}
		const PpCycleTimingWindow::Average& t = pendingTimingDetail;
		PpStressLine detail;
		detail.word("A det n=");
		detail.decimal(t.samples);
		detail.word(" polls=");
		detail.decimal(t.polls);
		detail.word(" ps=");
		detail.decimal(t.pollSpiUs);
		detail.word(" ob=");
		detail.decimal(t.observeUs);
		detail.word(" fi=");
		detail.decimal(t.fifoUs);
		detail.word(" dpl=");
		detail.decimal(t.payloadUs);
		detail.word(" irq=");
		detail.decimal(t.irqUs);
		detail.word(" ro=");
		detail.decimal(t.readOtherUs);
		detail.word(" src=");
		detail.decimal(t.sourceUs);
		detail.word(" enc=");
		detail.decimal(t.encodeUs);
		detail.word(" mo=");
		detail.decimal(t.prepareOtherUs);
		detail.word(" prf=");
		detail.decimal(t.protocolOverlapUs);
		detail.word(" rest=");
		detail.decimal(t.cpuOtherUs);
		detail.word("\r\n");
		if (detail.emit(uart)) {
			timingDetailPending = false;
		}
		return;
	}
	const uint32_t now = ticks.now();
	if (static_cast<uint32_t>(now - lastTimingReport) < PP_TIMING_REPORT_INTERVAL_TICKS) {
		return;
	}
	if (uart.freeSpace() < PP_TIMING_REPORT_MIN_FREE_BYTES) {
		return;
	}
	PpCycleTimingWindow::Average t;
	if (!cycleTiming.sample(t)) {
		return;
	}
	PpStressLine line;
	line.word("A us n=");
	line.decimal(t.samples);
	line.word(" cyc=");
	line.decimal(t.cycleUs);
	line.word(" spi=");
	line.decimal(t.loadUs);
	line.word(" grd=");
	line.decimal(t.guardUs);
	line.word(" ce=");
	line.decimal(t.ceUs);
	line.word(" rf=");
	line.decimal(t.waitUs);
	line.word(" ack=");
	line.decimal(t.readUs);
	line.word(" cpu=");
	line.decimal(t.cpuUs);
	line.word(" mk=");
	line.decimal(t.prepareUs);
	line.word(" pr=");
	line.decimal(t.protocolUs);
	line.word(" bg=");
	line.decimal(t.backgroundUs);
	line.word("\r\n");
	if (line.emit(uart)) {
		pendingTimingDetail = t;
		timingDetailPending = true;
		cycleTiming.clearWindow();
		lastTimingReport = now;
	}

#else
#endif
}

// One isolated timing site: the source/cache portion of stream.makeTraced.
void reportSourceProbe(uint32_t now) {
#if PP_SOURCE_PROBE_ENABLED

	if (static_cast<uint32_t>(now - lastSourceProbeReport) <
		PP_SOURCE_PROBE_REPORT_INTERVAL_TICKS) {
		return;
	}
	if (uart.freeSpace() < PP_SOURCE_PROBE_REPORT_MIN_FREE_BYTES) {
		return;
	}
	PpShortProbeWindow::Snapshot sample;
	if (!sourceProbe.sample(sample)) {
		return;
	}
	PpStressLine line;
	line.word("A hot n=");
	line.decimal(sample.count);
	line.word(" src=");
	line.decimal(sample.meanUs);
	line.word(" lo=");
	line.decimal(sample.minUs);
	line.word(" hi=");
	line.decimal(sample.maxUs);
	line.word("\r\n");
	if (line.emit(uart)) {
		sourceProbe.clear();
		lastSourceProbeReport = now;
	}

#else
	(void)now;
#endif
}

} // namespace PpApp
