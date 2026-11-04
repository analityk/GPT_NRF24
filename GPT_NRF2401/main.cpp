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
#include "boot_epoch.h"
#include "nrf24_ack_phy.h"
#include "pp_recovery_control.h"
#include "host_flow_status.h"

typedef Nrf24<SpiMaster,GPIO> Radio;
typedef NrfAckPayloadPhy<Radio,AvrDelay,Timer1Clock> AckPhy;
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
static uint32_t hardwareRetries=0,maxRetries=0,lastReport=0;
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

static void runA() {
    if(!phy.beginPtx())fatal();
    delay.ms(150); // one-time peer power-on, never per packet
    // Continuous DATA/ACK Payload ping-pong, even when neither UART has
    // bytes. The PHY alone enforces the 130us ESB turnaround guard.
    for(;;) {
        const uint32_t now=ticks.now();
        recovery.poll(now);
        if(session.active() && recovery.warmExpired(now)) {
            recovery.abandonWarm(now,stream.hasPending());
            if(!restartIdentity() || !phy.resetPtxFifos())fatal();
        }
        diagnosticPort.service(now,recovery.canProduce());
        // Once declared OFFLINE, only periodic IDLE/HELLO probes are sent.
        // No RAM is discarded, and we do not hammer MAX_RT indefinitely.
        if(session.active() &&
           recovery.state()==PpRecoveryControl::State::OFFLINE &&
           static_cast<uint32_t>(now-lastProbe)<40000UL) {
            reportPerformance();
            reportRecoveryTransition();
            delay.us(100);
            continue;
        }
        lastProbe=now;
        uint8_t width=0;
        if(!makePacket(packet,width))fatal();
        uint8_t incomingWidth=0;
        const AckPhy::TxResult tx=phy.transfer(packet,width,incoming,incomingWidth);
        linkState.notePtxRetries(tx.retries);
        hardwareRetries+=tx.retries;
        if(tx.code==AckPhy::TxCode::MAX_RT)++maxRetries;
        if(tx.code==AckPhy::TxCode::ACK_WITH_DATA)
            processFrame(incomingWidth);
        else if(tx.code==AckPhy::TxCode::BAD_REPLY_SIZE ||
                tx.code==AckPhy::TxCode::IO_ERROR ||
                tx.code==AckPhy::TxCode::TIMEOUT)
            ++radioErrors;

        // Negotiation timeout is different from ACTIVE radio silence.
        // Silence NEVER invalidates an established peer's session state.
        if(!session.active() &&
           session.phase()!=PairedSession::Phase::DISCOVER &&
           static_cast<uint32_t>(ticks.now()-lastPeerProgress)>2000000UL) {
            if(!restartIdentity() || !phy.resetPtxFifos())fatal();
        }
        reportPerformance();
        reportRecoveryTransition();
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
    if(radio.probeAndConfigure().result!=Radio::Result::OK)fatal();
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
    performance.start(lastReport,performanceSnapshot());
    if(isLeader)runA();
    else runB();
}
