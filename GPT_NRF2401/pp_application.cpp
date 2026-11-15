#include "pp_application.h"
#include "pp_application_internal.h"

namespace PpApp {
UART uart;
ActivePort diagnosticPort(uart);
SpiMaster spi;
GPIO ce(&PORTB, PB2), csn(&PORTB, PB1);
Radio radio(spi, ce, csn);
AvrDelay delay;
Timer1Clock ticks;
AckPhy phy(radio, delay, ticks);
PairedLink linkState(true, 0);
PairedSession session(true, PairedSession::Identity());
UartReliableStream stream(diagnosticPort);
bool isLeader = false;
uint32_t lastPeerProgress = 0;
// Bench stress mode explicitly retries uncertain bytes after a cold peer
// reset. The reusable default policy is HOLD until the host opts in.
PpRecoveryControl recovery(PP_RECOVERY_TURN_TICKS, PP_RECOVERY_WARM_TIMEOUT_TICKS,
	PpRecoveryControl::UncertainPolicy::RETRY_AT_LEAST_ONCE);
uint32_t lastProbe = 0;
#if PP_TEST_TRAFFIC_MODE == PP_TRAFFIC_MODE_SYNTHETIC_RADIO
PpRecoveryControl::State lastPrintedState = PpRecoveryControl::State::CONNECTING;
bool lastPrintedUncertainty = false;
#endif
uint32_t lastRecoveryReport = 0;

uint16_t recoveryCount = 0;
uint16_t radioErrors = 0;
bool replaceAck = false;
uint8_t packet[PairedFrame::MAX_FRAME];
uint8_t incoming[PairedFrame::MAX_FRAME];
uint8_t ackStaged[PairedFrame::MAX_FRAME];
uint8_t ackSize = 0;
bool ackWaiting = false;
#if PP_TEST_TRAFFIC_MODE == PP_TRAFFIC_MODE_SYNTHETIC_RADIO
PpStressWindow performance;
#endif
#if PP_PTX_TIMING_PROBE_ENABLED
PpCycleTimingWindow cycleTiming;
#endif
#if PP_SOURCE_PROBE_ENABLED
PpShortProbeWindow sourceProbe;
#endif
uint32_t lastSourceProbeReport = 0;
#if PP_PTX_TIMING_PROBE_ENABLED
PpCycleTimingWindow::Average pendingTimingDetail;
#endif
bool timingDetailPending = false;
uint32_t hardwareRetries = 0, maxRetries = 0, lastReport = 0;
uint32_t lastTimingReport = 0;
uint16_t ackFifoFull = 0;

} // namespace PpApp

void ppRun() {
	using namespace PpApp;
	uart.begin();
	Timer1Clock::begin();
	sei();
	ce.outputLow();
	csn.outputHigh();
	delay.ms(PP_TEST_RADIO_STARTUP_MS);
	if (radio.probeAndConfigure(PP_TEST_RF_SETUP_VALUE).result != Radio::Result::OK) {
		fatal();
	}
	spi.setSpeed(SpiMaster::Speed::MHz8);
	if (!radio.enableAckPayload(PP_TEST_ACK_RETRY_SETUP)) {
		fatal();
	}
	uint16_t adc = 0;
	if (!Adc0::sample(adc)) {
		fatal();
	}
	isLeader = (Adc0::classForM1(adc) == 0);
#if PP_TEST_TRAFFIC_MODE == PP_TRAFFIC_MODE_SYNTHETIC_RADIO
	diagnosticPort.configure(isLeader, ticks.now(), PpEchoRampPort::Mode::RADIO_STRESS);
#endif
	uint32_t epoch = 0;
	if (!BootEpoch::advance(epoch)) {
		fatal();
	}
	session.configure(isLeader, PairedSession::Identity(epoch, 0));
	linkState.configure(isLeader, 0);
	stream.disconnect();
	lastPeerProgress = ticks.now();
	recovery.reset(lastPeerProgress);
	lastProbe = lastPeerProgress;
	lastRecoveryReport = lastPeerProgress - PP_TEST_RECOVERY_REPORT_INITIAL_OFFSET_TICKS;
	lastReport = lastPeerProgress;
	lastTimingReport = lastReport - PP_TIMING_REPORT_PHASE_OFFSET_TICKS;
#if PP_TEST_TRAFFIC_MODE == PP_TRAFFIC_MODE_SYNTHETIC_RADIO
	performance.start(lastReport, performanceSnapshot());
#endif
	if (isLeader) {
		runA();
	} else {
		runB();
	}
}
