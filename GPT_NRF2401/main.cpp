#include "application_select.h"
#if !GPT_NRF24_ACK_BENCHMARK
/*
 * RF-Nano UART <-> reliable v1 UART endpoint over nRF24L01+.
 *
 * Both boards run EXACTLY the same firmware.
 * ADC0 to GND => class 0 (initiates each RF exchange).
 * ADC0 to own VCC => class 1 (listens and answers).
 *
 * Binary-transparent port: no local echo, no 'command' characters,
 * no text diagnostics mixed with user payload.
 * 8 B v1 header + up to 24 B body, fixed 32 B RF payload.
 * Session handshake: HELLO / HELLO_REPLY / READY / READY_ACK.
 * App transport: stop-and-wait ACK/duplicate filtering in both directions.
 *
 * This is still a staged M1 implementation: no hardware UART flow control,
 * no multi-node access, no exactly-once delivery across resets.
 */
#include "board_clock.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include "gpio.h"
#include "uart.h"
#include "spi.h"
#include "nrf24.h"
#include "nrf24_phy.h"
#include "avr_delay.h"
#include "adc0.h"
#include "frame_v1.h"
#include "uart_transport_v1.h"
#include "session_v1.h"
#include "boot_epoch.h"

typedef Nrf24<SpiMaster, GPIO> Radio;
typedef Nrf24Phy<Radio, AvrDelay> Phy;
typedef ReliableUartV1<UART> Bridge;

static UART uart;
static SpiMaster spi;
static GPIO radioCe(&PORTB, PB2);
static GPIO radioCsn(&PORTB, PB1);
static Radio radio(spi,radioCe,radioCsn);
static AvrDelay delay;
static Phy phy(radio,delay);
static Bridge bridge;
static SessionV1 session(0,1,1);

// One reusable RF packet buffer, no stack copies of 32-byte payloads.
static uint8_t packet[FrameV1::FRAME_SIZE];
static uint8_t body[FrameV1::BODY_SIZE];

// No UART debug output: use a debugger or later sideband telemetry.
// Counters saturate/wrap only as unsigned metrics; never affect delivery.
struct LinkCounters {
    uint16_t txAck,txMaxRt,txTimeout,txIoError,txRetries;
    uint16_t rxValid,rxMalformed,rxWrongSession;
    uint16_t replyMiss,linkRestarts;
    LinkCounters() : txAck(0),txMaxRt(0),txTimeout(0),
      txIoError(0),txRetries(0),rxValid(0),rxMalformed(0),
      rxWrongSession(0),replyMiss(0),linkRestarts(0) {}
};
static LinkCounters counters;

static void countTx(const Phy::TxResult& result) {
    if (result.result==Phy::TxStatus::ACKED) ++counters.txAck;
    else if (result.result==Phy::TxStatus::MAX_RETRIES) ++counters.txMaxRt;
    else if (result.result==Phy::TxStatus::TIMEOUT) ++counters.txTimeout;
    else ++counters.txIoError;
    counters.txRetries=static_cast<uint16_t>(counters.txRetries+result.arc);
}

static bool readFrame(FrameV1::View& view) {
    bool received=false;
    if (!phy.receive(packet,received)) {
        ++counters.rxMalformed; // includes SPI receive errors
        return false;
    }
    if (!received) return false;
    const FrameV1::Error status=FrameV1::decode(packet,view);
    if (status!=FrameV1::Error::OK) {
        ++counters.rxMalformed;
        return false;
    }
    return true;
}

static void freshTransport(uint16_t& seenGeneration,
                           uint8_t& txSeq,uint8_t& rxLast) {
    if (!session.active() || seenGeneration==session.generation()) return;
    seenGeneration=session.generation();
    bridge.newSession();
    txSeq=0;
    rxLast=0xFF;
}

// Both directions share the same radio packet; the transport sees only
// valid, correctly addressed and session-matched frames.
static void acceptActive(const FrameV1::View& v,uint8_t peer,uint8_t self,
                         uint8_t& rxLast) {
    const FrameV1::Header& h=v.header;
    if (!session.active() || h.src!=peer || h.dst!=self ||
        h.sid!=session.sid()) {
        ++counters.rxWrongSession;
        return;
    }
    rxLast=h.txSeq;
    ++counters.rxValid;
    bridge.accept(v,uart);
}

static bool encodeActive(uint8_t self,uint8_t peer,
                         uint8_t seq,uint8_t rxLast,
                         bool& hasAck,bool& hasData) {
    FrameV1::Header h;
    if (!bridge.prepare(uart,self,peer,session.sid(),seq,rxLast,h,body))
        return false;
    hasAck=(h.kind==FrameV1::Kind::CONTROL);
    hasData=(h.kind==FrameV1::Kind::DATA && h.length!=0) ||
            (h.kind==FrameV1::Kind::CONTROL && body[0]==0x12);
    return FrameV1::encode(h,body,packet)==FrameV1::Error::OK;
}

static Phy::TxResult sendFrame(bool hasAck,bool hasData) {
    const Phy::TxResult tx=phy.transmit(packet);
    countTx(tx);
    bridge.afterTransmit(tx.result==Phy::TxStatus::ACKED,
                         hasAck,hasData);
    return tx;
}

static void runLeader() {
    uint8_t seq=0,rxLast=0xFF;
    uint16_t generation=0;
    uint8_t misses=0;
    delay.ms(150);

    for (;;) {
        if (!session.active()) {
            if (!session.prepare(packet)) {
                delay.ms(20);
                continue;
            }
            const Phy::TxResult tx=phy.transmit(packet);
            countTx(tx);
        } else {
            bool ack=false,data=false;
            if (!encodeActive(0,1,seq,rxLast,ack,data)) {
                session.restart();
                ++counters.linkRestarts;
                continue;
            }
            (void)sendFrame(ack,data);
            ++seq;
        }

        bool gotReply=false;
        if (phy.listen()) {
            for (uint8_t ms=0;ms<100;++ms) {
                FrameV1::View v;
                if (readFrame(v)) {
                    if (!session.active()) {
                        if (session.accept(v)) {
                            gotReply=true;
                            freshTransport(generation,seq,rxLast);
                            break;
                        }
                    } else if (v.header.kind==FrameV1::Kind::CONTROL &&
                               v.body[0]==0x01) {
                        // A newly booted peer should never originate HELLO
                        // in M1 (the smaller class is the initiator).
                    } else if (v.header.src==1 && v.header.dst==0 &&
                               v.header.sid==session.sid()) {
                        acceptActive(v,1,0,rxLast);
                        gotReply=true;
                        break;
                    }
                }
                delay.ms(1);
            }
        }
        phy.standby();

        if (session.active() && bridge.needsRecovery()) {
            session.restart();
            ++counters.linkRestarts;
            misses=0;
        }
        if (session.active()) {
            if (!gotReply) {
                ++counters.replyMiss;
                if (++misses>=5) {
                    session.restart();
                    ++counters.linkRestarts;
                    misses=0;
                }
            } else misses=0;
        } else {
            misses=0;
        }
        delay.ms(session.active()?20:90);
    }
}

static void runFollower() {
    uint8_t seq=0,rxLast=0xFF;
    uint16_t generation=0;
    (void)phy.listen();
    for (;;) {
        FrameV1::View v;
        if (!readFrame(v)) {
            delay.ms(1);
            continue;
        }

        bool sendReply=false;
        bool handshaking=false;
        if (v.header.kind==FrameV1::Kind::CONTROL &&
            (v.body[0]==0x01 || v.body[0]==0x03)) {
            if (session.accept(v)) {
                freshTransport(generation,seq,rxLast);
                handshaking=session.prepare(packet);
                sendReply=handshaking;
            }
        } else if (session.active() && v.header.src==0 &&
                   v.header.dst==1 && v.header.sid==session.sid()) {
            acceptActive(v,0,1,rxLast);
            if (bridge.needsRecovery()) {
                session.restart();
                ++counters.linkRestarts;
                continue;
            }
            bool ack=false,data=false;
            if (encodeActive(1,0,seq,rxLast,ack,data)) {
                sendReply=true;
            }
            // v points into packet, which was overwritten above.
            if (sendReply) {
                // CE/RX must finish ACK before crossing to TX.
                delay.ms(25);
                const Phy::TxResult tx=sendFrame(ack,data);
                ++seq;
                (void)tx;
                (void)phy.listen();
            }
            continue;
        }

        if (sendReply) {
            delay.ms(25);
            const Phy::TxResult tx=phy.transmit(packet);
            countTx(tx);
            session.sentReply();
            (void)phy.listen();
        }
    }
}

int main() {
    uart.begin();
    sei();
    radioCe.clear();
    radioCsn.set();
    delay.ms(110);

    if (radio.probeAndConfigure().result!=Radio::Result::OK)
        for (;;) {delay.ms(100);} // do not emit error bytes on data port

    uint16_t adc=0;
    if (!Adc0::sample(adc))
        for (;;) {delay.ms(100);}
    const uint8_t self=Adc0::classForM1(adc);
    uint32_t epoch=0;
    if (!BootEpoch::advance(epoch))
        for (;;) {delay.ms(100);}
    session.configure(self,static_cast<uint8_t>(1U-self),epoch);

    if (session.isLeader()) runLeader();
    else runFollower();
}

#endif // !GPT_NRF24_ACK_BENCHMARK
