/*
 * TEMPORARY PP radio-saturation and error-recovery diagnostic.
 *
 * The very same AVR firmware runs on both boards. A0 low: A/PTX uses
 * UART 's'/'w' to adjust a generated 00..FF pattern (100us steps, from
 * maximum offered rate). A0 high: B/PRX queues each received RF body for
 * retransmission by hardware ACK Payload. BOTH UARTs print 1Hz counters;
 * no payload byte goes to UART, so UART baud does not limit radio throughput.
 * The negotiated radio SID and the hardware DATA/ACK Payload path remain.
 *
 * MAC TX_SEQ/RX_LAST carry user DATA_SEQ and cumulative acceptance ACK.
 * Every full DATA packet carries 24 raw UART bytes, with no BODY preamble.
 * Four in-flight segments per direction; no stop-and-wait or idle polling.
 *
 * The radio source queue is bounded and never overwrites pending bytes.
 * This experiment retains the existing session, MAC, window and PHY.
 * A silent peer is SUSPECT/OFFLINE, not automatically rebooted: only a
 * different peer handshake identity invalidates its volatile session.
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
#include "pp_stress_telemetry.h"
#include "pp_cycle_timing.h"
#include "boot_epoch.h"
#include "nrf24_ack_phy.h"
#include "pp_recovery_control.h"
#include "host_flow_status.h"

#define PP_PTX_PEER_STARTUP_MS 150U
#define PP_OFFLINE_PROBE_INTERVAL_TICKS 40000UL
#define PP_OFFLINE_IDLE_US 100U
#define PP_PTX_HANDSHAKE_TIMEOUT_TICKS 2000000UL
#define PP_PTX_BACKGROUND_MAX_GENERATED_BYTES PairedFrame::MAX_BODY
#define PP_TEST_RF_SETUP_VALUE NRF24_RF_SETUP_2_MBPS
#define PP_PTX_TIMING_PROBE_ENABLED true
#define PP_TIMING_REPORT_INTERVAL_TICKS 2000000UL
#define PP_TIMING_REPORT_PHASE_OFFSET_TICKS 1000000UL
#define PP_TIMING_REPORT_MIN_FREE_BYTES 120U

typedef Nrf24<SpiMaster,GPIO> Radio;
typedef NrfAckPayloadPhy<Radio,AvrDelay,Timer1Clock,
                        PP_PTX_TIMING_PROBE_ENABLED> AckPhy;
static UART uart;
static PpEchoRampPort<UART> diagnosticPort(uart);
static SpiMaster spi;
static GPIO ce(&PORTB,PB2),csn(&PORTB,PB1);
static Radio radio(spi,ce,csn);
static AvrDelay delay;
static Timer1Clock ticks;
static AckPhy phy(radio,delay,ticks);
static PairedLink linkState(true,0);
static PairedSession session(true,PairedSession::Identity());
static UartReliableStream<PpEchoRampPort<UART> > stream(diagnosticPort);
static bool isLeader=false;
static uint32_t lastPeerProgress=0;
// Bench stress mode explicitly retries uncertain bytes after a cold peer
// reset. The reusable default policy is HOLD until the host opts in.
static PpRecoveryControl recovery(3200UL,2000000UL,
                   PpRecoveryControl::UncertainPolicy::RETRY_AT_LEAST_ONCE);
static uint32_t lastProbe=0;
static PpRecoveryControl::State lastPrintedState=
                   PpRecoveryControl::State::CONNECTING;
static bool lastPrintedUncertainty=false;
static uint32_t lastRecoveryReport=0;

static uint16_t recoveryCount=0;
static uint16_t radioErrors=0;
static bool replaceAck=false;
static uint8_t packet[32],incoming[32];
static uint8_t ackStaged[32],ackSize=0;
static bool ackWaiting=false;
static PpStressWindow performance;
static PpCycleTimingWindow cycleTiming;
static uint32_t hardwareRetries=0,maxRetries=0,lastReport=0;
static uint32_t lastTimingReport=0;
static uint16_t ackFifoFull=0;

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

// Separate, short diagnostic line: per-exchange averages in microseconds.
// CPU time is the exclusive remainder; bg overlaps rf (not additive).
// ACK time includes SPI reading the payload and clearing the IRQ.
static void reportCycleTiming() {
    const uint32_t now=ticks.now();
    if(static_cast<uint32_t>(now-lastTimingReport)<
       PP_TIMING_REPORT_INTERVAL_TICKS)return;
    if(uart.freeSpace()<PP_TIMING_REPORT_MIN_FREE_BYTES)return;
    PpCycleTimingWindow::Average t;
    if(!cycleTiming.sample(t))return;
    PpStressLine line;
    line.word("A us n=");line.decimal(t.samples);
    line.word(" cyc=");line.decimal(t.cycleUs);
    line.word(" spi=");line.decimal(t.loadUs);
    line.word(" grd=");line.decimal(t.guardUs);
    line.word(" ce=");line.decimal(t.ceUs);
    line.word(" rf=");line.decimal(t.waitUs);
    line.word(" ack=");line.decimal(t.readUs);
    line.word(" cpu=");line.decimal(t.cpuUs);
    line.word(" mk=");line.decimal(t.prepareUs);
    line.word(" pr=");line.decimal(t.protocolUs);
    line.word(" bg=");line.decimal(t.backgroundUs);
    line.word("\r\n");
    if(line.emit(uart)) {
        cycleTiming.clearWindow();
        lastTimingReport=now;
    }
}

static void fatal() {
    // This temporary firmware owns the diagnostic UART, not a binary
    // payload stream. Make a halted test distinguishable from RF silence.
    const uint8_t marker[]={'F','A','T','A','L','\r','\n'};
    (void)uart.writeAll(marker,sizeof(marker));
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
static bool makePacket(uint8_t* dst,uint8_t& width) {
    if(!session.active())return session.prepare(dst,width);
    if(linkState.sessionId()!=session.sid()){
        linkState.configure(isLeader,session.sid());
        stream.beginSession(session.sid());
    }
    return stream.make(linkState,dst,width,recovery.canSendData());
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
    diagnosticPort.service(now,recovery.canProduce(),
                           PP_PTX_BACKGROUND_MAX_GENERATED_BYTES);
    reportPerformance();
    reportRecoveryTransition();
    if(PP_PTX_TIMING_PROBE_ENABLED)reportCycleTiming();
}

static void runA() {
    if(!phy.beginPtx())fatal();
    delay.ms(PP_PTX_PEER_STARTUP_MS); // one-time peer power-on
    bool servicedInFlight=false;
    uint8_t inFlightWidth=0;
    uint16_t prepareTicks=0,backgroundTicks=0,previousProtocolTicks=0;
    // P1: immediate SPI FIFO/IRQ service and CE transitions.
    // P2: other bounded work executes while the nRF is on air.
    for(;;) {
        if(phy.txBusy()) {
            uint8_t incomingWidth=0;
            AckPhy::TxResult tx;
            const AckPhy::TxStep step=
                phy.pollTransfer(incoming,incomingWidth,tx);
            if(step==AckPhy::TxStep::PENDING) {
                if(phy.txOnAir() && !servicedInFlight) {
                    const uint32_t workStart=PP_PTX_TIMING_PROBE_ENABLED?
                        ticks.now():0;
                    serviceAInFlight();
                    if(PP_PTX_TIMING_PROBE_ENABLED)
                        backgroundTicks=PpCycleTimingWindow::clampTicks(
                            static_cast<uint32_t>(ticks.now()-workStart));
                    servicedInFlight=true;
                }
                continue;
            }
            if(step!=AckPhy::TxStep::COMPLETE)fatal();
            const uint32_t protocolStart=PP_PTX_TIMING_PROBE_ENABLED?
                ticks.now():0;
            const bool wasActive=session.active();
            const uint16_t oldSid=session.sid();
            linkState.notePtxRetries(tx.retries);
            hardwareRetries+=tx.retries;
            if(tx.code==AckPhy::TxCode::MAX_RT)++maxRetries;
            if(tx.code==AckPhy::TxCode::ACK_WITH_DATA)
                processFrame(incomingWidth);
            else if(tx.code==AckPhy::TxCode::BAD_REPLY_SIZE ||
                    tx.code==AckPhy::TxCode::IO_ERROR ||
                    tx.code==AckPhy::TxCode::TIMEOUT)
                ++radioErrors;
            if(PP_PTX_TIMING_PROBE_ENABLED) {
                const uint16_t protocolTicks=PpCycleTimingWindow::clampTicks(
                    static_cast<uint32_t>(ticks.now()-protocolStart));
                const bool cleanFullExchange=wasActive && session.active() &&
                    oldSid==session.sid() &&
                    recovery.state()==PpRecoveryControl::State::HEALTHY &&
                    tx.code==AckPhy::TxCode::ACK_WITH_DATA && tx.retries==0 &&
                    inFlightWidth==PairedFrame::MAX_FRAME &&
                    incomingWidth==PairedFrame::MAX_FRAME;
                // Cycle n runs from completion n-1 to completion n.
                // Protocol processing after completion n belongs to n+1.
                cycleTiming.observe(phy.lastCompletedAt(),phy.lastTiming(),
                                    prepareTicks,previousProtocolTicks,
                                    backgroundTicks,cleanFullExchange);
                previousProtocolTicks=protocolTicks;
            }
            if(!servicedInFlight)serviceAInFlight();
            servicedInFlight=false;
            backgroundTicks=0;
            // Negotiation timeout is different from ACTIVE radio silence.
            // Silence NEVER invalidates a living peer's session state.
            if(!session.active() &&
               session.phase()!=PairedSession::Phase::DISCOVER &&
               static_cast<uint32_t>(ticks.now()-lastPeerProgress)>
                   PP_PTX_HANDSHAKE_TIMEOUT_TICKS) {
                if(!restartIdentity() || !phy.resetPtxFifos())fatal();
            }
            continue;
        }
        const uint32_t now=ticks.now();
        recovery.poll(now);
        if(session.active() && recovery.warmExpired(now)) {
            recovery.abandonWarm(now,stream.hasPending());
            if(!restartIdentity() || !phy.resetPtxFifos())fatal();
        }
        // While offline, preserve the cache and probe at a bounded rate.
        if(session.active() &&
           recovery.state()==PpRecoveryControl::State::OFFLINE &&
           static_cast<uint32_t>(now-lastProbe)<
               PP_OFFLINE_PROBE_INTERVAL_TICKS) {
            serviceAInFlight();
            delay.us(PP_OFFLINE_IDLE_US);
            continue;
        }
        lastProbe=now;
        uint8_t width=0;
        const uint32_t prepareStart=PP_PTX_TIMING_PROBE_ENABLED?
            ticks.now():0;
        if(!makePacket(packet,width))fatal();
        if(PP_PTX_TIMING_PROBE_ENABLED)
            prepareTicks=PpCycleTimingWindow::clampTicks(
                static_cast<uint32_t>(ticks.now()-prepareStart));
        inFlightWidth=width;
        if(!phy.startTransfer(packet,width)) {
            ++radioErrors;
            if(PP_PTX_TIMING_PROBE_ENABLED)cycleTiming.clearWindow();
            if(!phy.resetPtxFifos())fatal();
        }
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
    }
}

int main() {
    uart.begin();
    Timer1Clock::begin();
    sei();
    ce.clear();csn.set();
    delay.ms(110);
    if(radio.probeAndConfigure(PP_TEST_RF_SETUP_VALUE).result!=
       Radio::Result::OK)fatal();
    spi.setSpeed(SpiMaster::Speed::MHz8);
    if(!radio.enableAckPayload(0x23))fatal();
    uint16_t adc=0;
    if(!Adc0::sample(adc))fatal();
    isLeader=(Adc0::classForM1(adc)==0);
    diagnosticPort.configure(isLeader,ticks.now(),
        PpEchoRampPort<UART>::Mode::RADIO_STRESS);
    uint32_t epoch=0;
    if(!BootEpoch::advance(epoch))fatal();
    session.configure(isLeader,PairedSession::Identity(epoch,0));
    linkState.configure(isLeader,0);
    stream.disconnect();
    lastPeerProgress=ticks.now();
    recovery.reset(lastPeerProgress);
    lastProbe=lastPeerProgress;
    lastRecoveryReport=lastPeerProgress-400000UL;
    lastReport=lastPeerProgress;
    lastTimingReport=lastReport-PP_TIMING_REPORT_PHASE_OFFSET_TICKS;
    performance.start(lastReport,performanceSnapshot());
    if(isLeader)runA();
    else runB();
}
