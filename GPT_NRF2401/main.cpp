/*
 * Paired PP: same firmware for A/PTX and B/PRX (A0 role strap).
 * Compile-time traffic mode selects either a binary-transparent,
 * bidirectional USART0 bridge or the synthetic RF saturation benchmark.
 * MAC/session/window/recovery and ACK Payload are shared by both modes.
 * In bridge diagnostic builds UART prints a post-burst report only;
 * disable PP_UART_DIAG_ENABLED for byte-exact binary captures.
 */
#include "board_clock.h"
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "gpio.h"
#include "uart.h"
#include "spi.h"
#include "nrf24.h"
#include "avr_delay.h"
#include "adc0.h"
#include "timer1_clock.h"
#include "paired_frame.h"
#include "paired_link.h"
#include "paired_session.h"
#include "uart_reliable_stream.h"
#include "pp_echo_ramp_port.h"
#include "pp_uart_bridge_port.h"
#include "pp_test_traffic_mode.h"
#include "pp_stress_telemetry.h"
#include "pp_cycle_timing.h"
#include "pp_short_probe.h"
#include "pp_uart_quiet_diagnostic.h"
#include "boot_epoch.h"
#include "nrf24_ack_phy.h"
#include "pp_reply_mailbox.h"
#include "pp_recovery_control.h"
#include "host_flow_status.h"

#define PP_PTX_PEER_STARTUP_MS 150U
#define PP_TEST_RADIO_STARTUP_MS 110U
#define PP_TEST_ACK_RETRY_SETUP 0x23U
#define PP_TEST_RECOVERY_REPORT_INITIAL_OFFSET_TICKS 400000UL
#define PP_OFFLINE_PROBE_INTERVAL_TICKS 40000UL
#define PP_OFFLINE_IDLE_US 100U
#define PP_PTX_HANDSHAKE_TIMEOUT_TICKS 2000000UL
#define PP_PTX_BACKGROUND_MAX_GENERATED_BYTES PairedFrame::MAX_BODY
#define PP_TEST_RF_SETUP_VALUE NRF24_RF_SETUP_2_MBPS
#define PP_PTX_TIMING_PROBE_ENABLED false
#if PP_TEST_TRAFFIC_MODE == PP_TRAFFIC_MODE_UART_BRIDGE
#define PP_SOURCE_PROBE_ENABLED false
#else
#define PP_SOURCE_PROBE_ENABLED true
#endif
#define PP_SOURCE_PROBE_REPORT_INTERVAL_TICKS 2000000UL
#define PP_SOURCE_PROBE_REPORT_MIN_FREE_BYTES 80U
#define PP_TIMING_REPORT_INTERVAL_TICKS 2000000UL
#define PP_TIMING_REPORT_PHASE_OFFSET_TICKS 1000000UL
#define PP_TIMING_REPORT_MIN_FREE_BYTES 120U

typedef Nrf24<SpiMaster,GPIO> Radio;
typedef NrfAckPayloadPhy<Radio,AvrDelay,Timer1Clock,
                        PP_PTX_TIMING_PROBE_ENABLED> AckPhy;
static UART uart;
#if PP_TEST_TRAFFIC_MODE == PP_TRAFFIC_MODE_UART_BRIDGE
typedef PpUartBridgePort<UART> ActivePort;
#else
typedef PpEchoRampPort<UART> ActivePort;
#endif
static ActivePort diagnosticPort(uart);
static SpiMaster spi;
static GPIO ce(&PORTB,PB2),csn(&PORTB,PB1);
static Radio radio(spi,ce,csn);
static AvrDelay delay;
static Timer1Clock ticks;
static AckPhy phy(radio,delay,ticks);
static PairedLink linkState(true,0);
static PairedSession session(true,PairedSession::Identity());
static UartReliableStream<ActivePort,
	PP_TEST_TRAFFIC_MODE==PP_TRAFFIC_MODE_UART_BRIDGE> stream(diagnosticPort);
static bool isLeader=false;
static uint32_t lastPeerProgress=0;
// Bench stress mode explicitly retries uncertain bytes after a cold peer
// reset. The reusable default policy is HOLD until the host opts in.
static PpRecoveryControl recovery(3200UL,2000000UL,
                   PpRecoveryControl::UncertainPolicy::RETRY_AT_LEAST_ONCE);
static uint32_t lastProbe=0;
#if PP_TEST_TRAFFIC_MODE == PP_TRAFFIC_MODE_SYNTHETIC_RADIO
static PpRecoveryControl::State lastPrintedState=
	PpRecoveryControl::State::CONNECTING;
static bool lastPrintedUncertainty=false;
#endif
static uint32_t lastRecoveryReport=0;

static uint16_t recoveryCount=0;
static uint16_t radioErrors=0;
static bool replaceAck=false;
static uint8_t packet[32],incoming[32];
static uint8_t ackStaged[32],ackSize=0;
static bool ackWaiting=false;
static PpStressWindow performance;
static PpCycleTimingWindow cycleTiming;
static PpShortProbeWindow sourceProbe;
static uint32_t lastSourceProbeReport=0;
static PpCycleTimingWindow::Average pendingTimingDetail;
static bool timingDetailPending=false;
static uint32_t hardwareRetries=0,maxRetries=0,lastReport=0;
static uint32_t lastTimingReport=0;
static uint16_t ackFifoFull=0;

#if PP_UART_DIAG_ENABLED
static PpUartQuietDiagnostic uartQuietDiagnostic;
static PpUartDiagReport queuedDiagnostic;
static uint32_t uartDiagnosticPollAt=0;
static uint16_t diagnosticTxBytes=0;
static PpShortProbeWindow rfCycleProbe;
static uint16_t previousRfStart=0;
static bool previousRfStartValid=false;
static bool previousRfDataExchange=false;

enum class UartDiagnosticPhase : uint8_t {
	OBSERVE,UART_COUNTS,PROTOCOL_COUNTS,RF_CYCLE
};
static UartDiagnosticPhase uartDiagnosticPhase=UartDiagnosticPhase::OBSERVE;

// One timing region: PTX start-to-start after a successful radio exchange
// carrying DATA in at least one direction (variable payload lengths).
static void markRfCycle(uint16_t started,bool successful){
	if(!successful){
		previousRfStartValid=false;
		previousRfDataExchange=false;
		return;
	}
	if(previousRfStartValid && previousRfDataExchange){
		rfCycleProbe.observe(static_cast<uint16_t>(
			started-previousRfStart));
	}
	previousRfStart=started;
	previousRfStartValid=true;
	previousRfDataExchange=false;
}

static PpUartDiagSnapshot uartDiagnosticSnapshot(){
	const UART::Counters counters=uart.counters();
	PpUartDiagSnapshot snapshot;
	snapshot.uartRx=counters.rx_bytes;
	// Subtract ASCII diagnostics from the physical UART TX count.
	snapshot.uartTx=static_cast<uint16_t>(
		counters.tx_bytes-diagnosticTxBytes);
	snapshot.rxFull=counters.rx_buffer_full;
	snapshot.rxOverrun=counters.rx_overrun;
	snapshot.rxFraming=counters.rx_framing;
	snapshot.acknowledged=stream.acknowledgedBytes();
	snapshot.accepted=stream.acceptedBytes();
	snapshot.retries=stream.retransmits();
	snapshot.blocked=stream.blocked();
	snapshot.bad=stream.badFrames();
	snapshot.hwArc=static_cast<uint16_t>(hardwareRetries);
	snapshot.maxRt=static_cast<uint16_t>(maxRetries);
	snapshot.ackFifoFull=ackFifoFull;
	snapshot.rxFifoFull=phy.rxFifoFullSamples();
	snapshot.sid=session.sid();
	snapshot.outstanding=stream.outstanding();
	snapshot.credit=linkState.peerCredit();
	snapshot.freeInput=stream.inputFreeSpace();
	return snapshot;
}

// Only the post-burst quiet interval allows a diagnostic ASCII report.
// UART input and RF output never carry any additional in-band commands.
static void serviceUartDiagnostic(){
	if(uartDiagnosticPhase==UartDiagnosticPhase::OBSERVE){
		const uint32_t now=ticks.now();
		if(static_cast<uint32_t>(now-uartDiagnosticPollAt)<
			PP_UART_DIAG_POLL_TICKS){
			return;
		}
		uartDiagnosticPollAt=now;
		if(!uartQuietDiagnostic.observe(now,uartDiagnosticSnapshot(),
			queuedDiagnostic)){
			return;
		}
		uartDiagnosticPhase=UartDiagnosticPhase::UART_COUNTS;
	}
	if(uart.freeSpace()<PP_UART_DIAG_LINE_FREE_BYTES){
		return;
	}
	PpStressLine line;
	if(uartDiagnosticPhase==UartDiagnosticPhase::UART_COUNTS){
		line.word(isLeader?"A uart in=":"B uart in=");
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
	} else if(uartDiagnosticPhase==UartDiagnosticPhase::PROTOCOL_COUNTS){
		line.word(isLeader?"A pp ack=":"B pp ack=");
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
		if(isLeader){
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
	} else {
		PpShortProbeWindow::Snapshot cycle;
		if(rfCycleProbe.sample(cycle)){
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
	}
	if(!line.emit(uart)){
		return;
	}
	diagnosticTxBytes=static_cast<uint16_t>(
		diagnosticTxBytes+line.length());
	if(uartDiagnosticPhase==UartDiagnosticPhase::UART_COUNTS){
		uartDiagnosticPhase=UartDiagnosticPhase::PROTOCOL_COUNTS;
	} else if(uartDiagnosticPhase==UartDiagnosticPhase::PROTOCOL_COUNTS &&
		isLeader){
		uartDiagnosticPhase=UartDiagnosticPhase::RF_CYCLE;
	} else {
		uartDiagnosticPhase=UartDiagnosticPhase::OBSERVE;
		rfCycleProbe.clear();
	}
}
#else
static void serviceUartDiagnostic(){
}
#endif


#if PP_TEST_TRAFFIC_MODE == PP_TRAFFIC_MODE_SYNTHETIC_RADIO
static void reportRecoveryTransition() {
    const PpRecoveryControl::State state=recovery.state();
    const bool uncertain=recovery.uncertainty();
    if(state==lastPrintedState && uncertain==lastPrintedUncertainty)return;
    const uint32_t now=ticks.now();
    // Do not let rapid HEALTHY/SUSPECT changes saturate the console or
    // compete with RF throughput. Sample the latest state at most 5 Hz.
    if(static_cast<uint32_t>(now-lastRecoveryReport)<400000UL)return;
    lastRecoveryReport=now; // also throttle attempts if UART ring is full
    const HostFlowStatus::Pair status=HostFlowStatus::snapshot(
        recovery,diagnosticPort.inputFreeSpace(),diagnosticPort.freeSpace());
    PpStressLine line;
    line.word(isLeader?"A link=":"B link=");
    line.decimal(static_cast<uint8_t>(state));
    line.word(" tx=");line.decimal(status.tx);
    line.word(" rx=");line.decimal(status.rx);
    line.word(" warm=");line.decimal(recovery.warmRecoveries());
    line.word(" cold=");line.decimal(recovery.coldRecoveries());
    line.word(" exp=");line.decimal(recovery.expiredRecoveries());
    line.word(" uncertain=");line.decimal(uncertain?1U:0U);
    line.word("\r\n");
    if(line.emit(uart)) {
        lastPrintedState=state;
        lastPrintedUncertainty=uncertain;
    }
}

static PpStressSnapshot performanceSnapshot() {
    PpStressSnapshot s;
    s.sourceBytes=isLeader?diagnosticPort.generated():diagnosticPort.forwarded();
    s.deliveredBytes=isLeader?diagnosticPort.echoed():diagnosticPort.received();
    s.hardwareRetries=hardwareRetries;
    s.maxRetries=maxRetries;
    s.echoGaps=diagnosticPort.echoGaps();
    s.appRetries=stream.retransmits();
    s.duplicates=stream.duplicates();
    s.badFrames=stream.badFrames();
    s.blocked=stream.blocked();
    s.radioErrors=radioErrors;
    s.recoveries=recoveryCount;
    s.acknowledgedBytes=stream.acknowledgedBytes();
    s.outstanding=stream.outstanding();
    s.linkGaps=static_cast<uint16_t>(linkState.counters().gaps);
    s.linkStale=static_cast<uint16_t>(linkState.counters().stale);
    s.rxFifoFull=phy.rxFifoFullSamples();
    s.ackFifoFull=ackFifoFull;
    s.sid=session.sid();
    s.active=session.active();
    return s;
}

static void reportPerformance() {
    const uint32_t now=ticks.now();
    if(static_cast<uint32_t>(now-lastReport)<2000000UL)return;
    lastReport=now;
    const PpStressSnapshot snapshot=performanceSnapshot();
    PpStressSample sample;
    if(performance.take(now,snapshot,sample))
        (void)printPpStressLine(uart,isLeader,sample);
}

#else
static void reportRecoveryTransition(){
}

static void reportPerformance(){
}
#endif

// Separate, short diagnostic line: per-exchange averages in microseconds.
// CPU time is the exclusive remainder; bg and the deferred part of pr
// overlap rf (they must not be added to the total exchange period).
// ACK time includes SPI reading the payload and clearing the IRQ.
static void reportCycleTiming(){
	// The UART ring cannot fit both lines at once. Queue the second line
	// only when transmission of the first has freed enough space; the
	// saved sample still refers to the SAME set of RF exchanges.
	if(timingDetailPending){
		if(uart.freeSpace()<PP_TIMING_REPORT_MIN_FREE_BYTES){
			return;
		}
		const PpCycleTimingWindow::Average& t=pendingTimingDetail;
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
		if(detail.emit(uart)){
			timingDetailPending=false;
		}
		return;
	}
	const uint32_t now=ticks.now();
	if(static_cast<uint32_t>(now-lastTimingReport)<
		PP_TIMING_REPORT_INTERVAL_TICKS){
		return;
	}
	if(uart.freeSpace()<PP_TIMING_REPORT_MIN_FREE_BYTES){
		return;
	}
	PpCycleTimingWindow::Average t;
	if(!cycleTiming.sample(t)){
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
	if(line.emit(uart)){
		pendingTimingDetail=t;
		timingDetailPending=true;
		cycleTiming.clearWindow();
		lastTimingReport=now;
	}
}

// One isolated timing site: the source/cache portion of stream.makeTraced.
static void reportSourceProbe(uint32_t now){
	if(static_cast<uint32_t>(now-lastSourceProbeReport)<
		PP_SOURCE_PROBE_REPORT_INTERVAL_TICKS){
		return;
	}
	if(uart.freeSpace()<PP_SOURCE_PROBE_REPORT_MIN_FREE_BYTES){
		return;
	}
	PpShortProbeWindow::Snapshot sample;
	if(!sourceProbe.sample(sample)){
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
	if(line.emit(uart)){
		sourceProbe.clear();
		lastSourceProbeReport=now;
	}
}

static void fatal() {
    // This temporary firmware owns the diagnostic UART, not a binary
    // payload stream. Make a halted test distinguishable from RF silence.
#if PP_TEST_TRAFFIC_MODE == PP_TRAFFIC_MODE_SYNTHETIC_RADIO
    const uint8_t marker[]={'F','A','T','A','L','\r','\n'};
    (void)uart.writeAll(marker,sizeof(marker));
#endif
    for(;;)delay.ms(100);
}
static bool restartIdentity() {
    if(!session.restart()){
        uint32_t next=0;
        if(!BootEpoch::advance(next))return false;
        session.changeBoot(next);
    }
    linkState.configure(isLeader,0);
    stream.disconnect(); // Retains the pending, unacknowledged UART segment.
    ++recoveryCount;
    lastPeerProgress=ticks.now();
    return true;
}
template<class Trace>
static bool makePacketTraced(uint8_t* dst,uint8_t& width,Trace& trace) {
    if(!session.active())return session.prepare(dst,width);
    if(linkState.sessionId()!=session.sid()){
        linkState.configure(isLeader,session.sid());
        stream.beginSession(session.sid());
    }
    return stream.makeTraced(linkState,dst,width,
                             recovery.canSendData(),trace);
}
static bool makePacket(uint8_t* dst,uint8_t& width) {
    UartMakeNoopTrace trace;
    return makePacketTraced(dst,width,trace);
}

// Handshake must be decoded BEFORE the strict active-session filter.
// Hardware ACK is never interpreted here as acceptance of UART bytes.
static void processFrame(uint8_t width) {
    PairedFrame::View view;
    if(PairedFrame::decode(incoming,width,view)!=PairedFrame::Error::OK){
        ++radioErrors;
        return;
    }
    const PairedSession::Event event=session.accept(view);
    if(event!=PairedSession::Event::NONE) {
        const uint32_t now=ticks.now();
        lastPeerProgress=now;
        if(event==PairedSession::Event::LOST) {
            recovery.peerStateLost(now,stream.hasPending());
            if(isLeader && session.active()){
                uint32_t epoch=0;
                if(!BootEpoch::advance(epoch))fatal();
                session.changeBoot(epoch);
            }
            linkState.configure(isLeader,0);
            stream.disconnect();
            ++recoveryCount;
            if(isLeader && !phy.resetPtxFifos())++radioErrors;
        } else if(event==PairedSession::Event::ESTABLISHED) {
            linkState.configure(isLeader,session.sid());
            stream.beginSession(session.sid());
            recovery.established(now);
        } else if(!session.active()) {
            linkState.configure(isLeader,0);
            stream.disconnect();
        }
        // PRX must invalidate any preloaded ACK from a prior phase/epoch.
        if(!isLeader)replaceAck=true;
    }
    if(view.header.kind()==PairedFrame::Kind::CONTROL)return;
    if(!session.active() || view.header.sessionId!=session.sid())
        return;
    const PairedLink::Receipt receipt=linkState.observeStream(view);
    if(receipt==PairedLink::Receipt::WRONG_SESSION)return;
    const uint32_t now=ticks.now();
    lastPeerProgress=now;
    recovery.validPeerFrame(now);
    // A repeated DATA sequence may carry a NEW application ACK.
    (void)stream.accept(view);
}

// Priority 2: bounded application/diagnostic work while the nRF itself
// performs the physical DATA/ACK exchange. Protocol state is not modified
// by an unprocessed ACK: the previous reply is always consumed before the
// next stream.make(), protecting credit, sequence and rekey ordering.
static void serviceAInFlight() {
    const uint32_t now=ticks.now();
    recovery.poll(now);
#if PP_TEST_TRAFFIC_MODE == PP_TRAFFIC_MODE_SYNTHETIC_RADIO
    diagnosticPort.service(now,recovery.canProduce(),
                           PP_PTX_BACKGROUND_MAX_GENERATED_BYTES);
#endif
    reportPerformance();
    reportRecoveryTransition();
    if(PP_PTX_TIMING_PROBE_ENABLED){
		reportCycleTiming();
	}
	if(PP_SOURCE_PROBE_ENABLED){
		reportSourceProbe(now);
	}
	serviceUartDiagnostic();
}

// The PHY completed the hardware exchange. The MAC completion can be
// delayed one turn, but hardware retry accounting cannot be skipped.
static void processAReply(const AckPhy::TxResult& tx,uint8_t width){
	if(tx.code==AckPhy::TxCode::ACK_WITH_DATA){
		processFrame(width);
	} else if(tx.code==AckPhy::TxCode::BAD_REPLY_SIZE ||
		tx.code==AckPhy::TxCode::IO_ERROR ||
		tx.code==AckPhy::TxCode::TIMEOUT){
		++radioErrors;
	}
}

static void runA(){
	if(!phy.beginPtx()){
		fatal();
	}
	delay.ms(PP_PTX_PEER_STARTUP_MS);
	PpReplyMailbox<AckPhy::TxResult> deferredReply;
	bool servicedInFlight=false;
	uint8_t inFlightWidth=0;
	uint16_t prepareTicks=0;
	uint16_t backgroundTicks=0;
	uint16_t previousProtocolTicks=0;
	uint16_t protocolOverlapTicks=0;
	uint16_t sourceTicks=0;
	uint16_t encodeTicks=0;
	for(;;){
		if(phy.txBusy()){
			// The previous SPI RX buffer must be consumed BEFORE another
			// pollTransfer could overwrite it with the current ACK payload.
			// Starting the new RF turn first moves this work under ON_AIR.
			if(phy.txOnAir() && !servicedInFlight){
				if(deferredReply.pending()){
					AckPhy::TxResult previous;
					uint8_t previousWidth=0;
					if(!deferredReply.take(previous,previousWidth)){
						fatal();
					}
					const uint32_t protocolStart=
						PP_PTX_TIMING_PROBE_ENABLED?ticks.now():0;
					processAReply(previous,previousWidth);
					if(PP_PTX_TIMING_PROBE_ENABLED){
						protocolOverlapTicks=PpCycleTimingWindow::clampTicks(
							static_cast<uint32_t>(ticks.now()-protocolStart));
					}
				}
				const uint32_t workStart=
					PP_PTX_TIMING_PROBE_ENABLED?ticks.now():0;
				serviceAInFlight();
				if(PP_PTX_TIMING_PROBE_ENABLED){
					backgroundTicks=PpCycleTimingWindow::clampTicks(
						static_cast<uint32_t>(ticks.now()-workStart));
				}
				servicedInFlight=true;
			}
			uint8_t incomingWidth=0;
			AckPhy::TxResult tx;
			const AckPhy::TxStep step=
				phy.pollTransfer(incoming,incomingWidth,tx);
			if(step==AckPhy::TxStep::PENDING){
				continue;
			}
			if(step!=AckPhy::TxStep::COMPLETE || deferredReply.pending()){
				fatal();
			}
			const bool wasActive=session.active();
			const uint16_t oldSid=session.sid();
			linkState.notePtxRetries(tx.retries);
			hardwareRetries+=tx.retries;
			if(tx.code==AckPhy::TxCode::MAX_RT){
				++maxRetries;
			}
			// A control/foreign/invalid reply can trigger a session reset
			// and must be processed before any later payload is queued.
#if PP_UART_DIAG_ENABLED
			previousRfDataExchange=wasActive && session.active() &&
				oldSid==session.sid() &&
				recovery.state()==PpRecoveryControl::State::HEALTHY &&
				tx.code==AckPhy::TxCode::ACK_WITH_DATA &&
				tx.retries==0 &&
				(inFlightWidth>PairedFrame::HEADER_SIZE ||
				 incomingWidth>PairedFrame::HEADER_SIZE);
#endif
			const bool defer=tx.code==AckPhy::TxCode::ACK_WITH_DATA &&
				PpReplyMailbox<AckPhy::TxResult>::canDefer(
					session.active(),session.sid(),incoming,incomingWidth);
			uint16_t immediateProtocolTicks=0;
			if(defer){
				if(!deferredReply.store(tx,incomingWidth)){
					fatal();
				}
			} else {
				const uint32_t protocolStart=
					PP_PTX_TIMING_PROBE_ENABLED?ticks.now():0;
				processAReply(tx,incomingWidth);
				if(PP_PTX_TIMING_PROBE_ENABLED){
					immediateProtocolTicks=PpCycleTimingWindow::clampTicks(
						static_cast<uint32_t>(ticks.now()-protocolStart));
				}
			}
			if(PP_PTX_TIMING_PROBE_ENABLED){
				const bool cleanFullExchange=wasActive && session.active() &&
					oldSid==session.sid() &&
					recovery.state()==PpRecoveryControl::State::HEALTHY &&
					tx.code==AckPhy::TxCode::ACK_WITH_DATA && tx.retries==0 &&
					inFlightWidth==PairedFrame::MAX_FRAME &&
					incomingWidth==PairedFrame::MAX_FRAME;
				const uint16_t protocolTicks=PpCycleTimingWindow::clampTicks(
					static_cast<uint32_t>(previousProtocolTicks)+
					protocolOverlapTicks);
				cycleTiming.observe(phy.lastCompletedAt(),phy.lastTiming(),
					prepareTicks,protocolTicks,backgroundTicks,
					sourceTicks,encodeTicks,protocolOverlapTicks,
					cleanFullExchange);
				previousProtocolTicks=immediateProtocolTicks;
			}
			protocolOverlapTicks=0;
			if(!servicedInFlight){
				serviceAInFlight();
			}
			servicedInFlight=false;
			backgroundTicks=0;
			if(!session.active() &&
				session.phase()!=PairedSession::Phase::DISCOVER &&
				static_cast<uint32_t>(ticks.now()-lastPeerProgress)>
				PP_PTX_HANDSHAKE_TIMEOUT_TICKS){
				if(!restartIdentity() || !phy.resetPtxFifos()){
					fatal();
				}
			}
			continue;
		}
		const uint32_t now=ticks.now();
		recovery.poll(now);
		if(session.active() && recovery.warmExpired(now)){
			recovery.abandonWarm(now,stream.hasPending());
			if(!restartIdentity() || !phy.resetPtxFifos()){
				fatal();
			}
		}
		if(session.active() &&
			recovery.state()==PpRecoveryControl::State::OFFLINE &&
			static_cast<uint32_t>(now-lastProbe)<
			PP_OFFLINE_PROBE_INTERVAL_TICKS){
			serviceAInFlight();
			delay.us(PP_OFFLINE_IDLE_US);
			continue;
		}
		lastProbe=now;
		uint8_t width=0;
		const uint32_t prepareStart=
			PP_PTX_TIMING_PROBE_ENABLED?ticks.now():0;
#if PP_SOURCE_PROBE_ENABLED
		PpShortSourceTrace<Timer1Clock> makeTrace(ticks);
#else
		PpFrameMakeTrace<Timer1Clock,PP_PTX_TIMING_PROBE_ENABLED> makeTrace(
			ticks,prepareStart);
#endif
		if(!makePacketTraced(packet,width,makeTrace)){
			fatal();
		}
#if PP_SOURCE_PROBE_ENABLED
		if(session.active() && width==PairedFrame::MAX_FRAME &&
			recovery.state()==PpRecoveryControl::State::HEALTHY &&
			makeTrace.sampled()){
			sourceProbe.observe(makeTrace.sourceTicks());
		}
#endif
		if(PP_PTX_TIMING_PROBE_ENABLED){
			prepareTicks=PpCycleTimingWindow::clampTicks(
				static_cast<uint32_t>(ticks.now()-prepareStart));
		}
		sourceTicks=makeTrace.sourceTicks();
		encodeTicks=makeTrace.encodeTicks();
		inFlightWidth=width;
#if PP_UART_DIAG_ENABLED
		const uint16_t rfCycleStart=ticks.counter16();
#endif
		if(!phy.startTransfer(packet,width)){
#if PP_UART_DIAG_ENABLED
			markRfCycle(rfCycleStart,false);
#endif
			++radioErrors;
			if(PP_PTX_TIMING_PROBE_ENABLED){
				cycleTiming.clearWindow();
			}
			if(!phy.resetPtxFifos()){
				fatal();
			}
			// The deferred ACK is still in incoming and must be consumed
			// before reusing the buffer on the next successful transfer.
		}
#if PP_UART_DIAG_ENABLED
		else {
			markRfCycle(rfCycleStart,true);
		}
#endif
	}
}

static bool queueB() {
    if(replaceAck) {
        ackWaiting=false;
        if(!makePacket(ackStaged,ackSize))return false;
        if(!phy.replacePrxReply(ackStaged,ackSize))return false;
        replaceAck=false;
        return true;
    }
    if(!ackWaiting) {
        if(!makePacket(ackStaged,ackSize))return false;
        ackWaiting=true;
    }
    const AckPhy::QueueCode q=phy.queueReply(ackStaged,ackSize);
    if(q==AckPhy::QueueCode::QUEUED) {
        ackWaiting=false;
        return true;
    }
    if(q==AckPhy::QueueCode::FULL)++ackFifoFull;
    if(q==AckPhy::QueueCode::IO_ERROR)++radioErrors;
    // FULL is normal when the PRX has a pipelined response outstanding.
    return false;
}

static void runB() {
    if(!phy.beginPrx())fatal();
    if(!queueB())fatal(); // initial BEACON for A's first HELLO
    for(;;) {
        const uint32_t now=ticks.now();
        recovery.poll(now);
        if(session.active() && recovery.warmExpired(now)) {
            recovery.abandonWarm(now,stream.hasPending());
            if(!restartIdentity())fatal();
            replaceAck=true;
        }
        uint8_t incomingWidth=0;
        const AckPhy::RxCode rx=phy.receive(incoming,incomingWidth);
        if(rx==AckPhy::RxCode::GOOD) {
            processFrame(incomingWidth);
            if(!phy.clearPrxFlags())++radioErrors;
            (void)queueB();
        } else if(rx==AckPhy::RxCode::BAD_SIZE ||
                  rx==AckPhy::RxCode::IO_ERROR) {
            ++radioErrors;
            if(!phy.clearPrxFlags())++radioErrors;
        }
        if(!session.active() &&
           session.phase()!=PairedSession::Phase::DISCOVER &&
           static_cast<uint32_t>(ticks.now()-lastPeerProgress)>4000000UL) {
            if(!restartIdentity())fatal();
            replaceAck=true;
        }
        if(replaceAck && !queueB())++radioErrors;
        reportPerformance();
        reportRecoveryTransition();
		serviceUartDiagnostic();
    }
}

int main(){
	uart.begin();
	Timer1Clock::begin();
	sei();
	ce.outputLow();
	csn.outputHigh();
	delay.ms(PP_TEST_RADIO_STARTUP_MS);
	if(radio.probeAndConfigure(PP_TEST_RF_SETUP_VALUE).result !=
		Radio::Result::OK){
		fatal();
	}
	spi.setSpeed(SpiMaster::Speed::MHz8);
	if(!radio.enableAckPayload(PP_TEST_ACK_RETRY_SETUP)){
		fatal();
	}
	uint16_t adc=0;
	if(!Adc0::sample(adc)){
		fatal();
	}
	isLeader=(Adc0::classForM1(adc)==0);
#if PP_TEST_TRAFFIC_MODE == PP_TRAFFIC_MODE_SYNTHETIC_RADIO
	diagnosticPort.configure(isLeader,ticks.now(),
		PpEchoRampPort<UART>::Mode::RADIO_STRESS);
#endif
	uint32_t epoch=0;
	if(!BootEpoch::advance(epoch)){
		fatal();
	}
	session.configure(isLeader,PairedSession::Identity(epoch,0));
	linkState.configure(isLeader,0);
	stream.disconnect();
	lastPeerProgress=ticks.now();
	recovery.reset(lastPeerProgress);
	lastProbe=lastPeerProgress;
	lastRecoveryReport=lastPeerProgress-
		PP_TEST_RECOVERY_REPORT_INITIAL_OFFSET_TICKS;
	lastReport=lastPeerProgress;
	lastTimingReport=lastReport-PP_TIMING_REPORT_PHASE_OFFSET_TICKS;
#if PP_TEST_TRAFFIC_MODE == PP_TRAFFIC_MODE_SYNTHETIC_RADIO
	performance.start(lastReport,performanceSnapshot());
#endif
	if(isLeader){
		runA();
	} else {
		runB();
	}
}
