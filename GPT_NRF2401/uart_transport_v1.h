#ifndef GPT_NRF24_UART_TRANSPORT_V1_H
#define GPT_NRF24_UART_TRANSPORT_V1_H

#include <stdint.h>
#include "frame_v1.h"

// Stop-and-wait UART endpoint (1) in both directions.
// Port: available(), read(uint8_t&), freeSpace(), writeAll(ptr,count).
// No ownership of hardware, no timers, no heap and no AVR-specific code.
//
// One segment is held until an application ACK (NOT nRF TX_DS).
// A DATA_ACK simultaneously acknowledges incoming data and sends outgoing
// data; staging is deliberately capped at 18 B to avoid starvation when
// both UARTs are active. Standalone DATA can support 23 B on the wire.
template <typename Port>
class ReliableUartV1 {
public:
    static const uint8_t ENDPOINT=1;
    static const uint8_t MAX_SEGMENT=18;
    enum Ack : uint8_t { ACCEPTED=0, BUSY=1, REJECTED=2 };

    struct Statistics {
        uint16_t segmentsQueued;
        uint16_t segmentsAccepted;
        uint16_t bytesAccepted;
        uint16_t retransmitted;
        uint16_t duplicateRx;
        uint16_t rxBusy;
        uint16_t rxRejected;
        uint16_t ackBusy;
        uint16_t ackRejected;
        Statistics() : segmentsQueued(0),segmentsAccepted(0),
          bytesAccepted(0),retransmitted(0),duplicateRx(0),
          rxBusy(0),rxRejected(0),ackBusy(0),ackRejected(0) {}
    };

    ReliableUartV1() : outstanding_(false), pendingLength_(0),
        sendSeq_(0), nextExpected_(0), acceptedLast_(false),
        ackPending_(false), ackSeq_(0), ackStatus_(0), txAttempts_(0) {}

    // Called only AFTER a newly negotiated session is accepted. Pending
    // unacknowledged bytes are retained and retried as segment zero.
    // May duplicate bytes delivered just before a peer reboot. Exactly-once
    // over a reset requires persistent delivery journaling and is not claimed.
    void newSession() {
        sendSeq_=0;
        nextExpected_=0;
        acceptedLast_=false;
        ackPending_=false;
        txAttempts_=0;
    }

    bool hasOutstanding() const { return outstanding_; }
    const Statistics& stats() const { return stats_; }

    // Build a complete v1 header/body for one scheduled TX slot.
    // No UART data is consumed when a segment is already outstanding.
    bool prepare(Port& port, uint8_t src, uint8_t dst,
                 uint8_t sid, uint8_t radioSeq, uint8_t lastRx,
                 FrameV1::Header& h, uint8_t (&body)[24]) {
        if (!outstanding_) stage(port);

        h=FrameV1::Header();
        h.src=src; h.dst=dst; h.sid=sid;
        h.txSeq=radioSeq; h.rxLast=lastRx;
        for (uint8_t i=0; i<24; ++i) body[i]=0;

        if (ackPending_) {
            h.kind=FrameV1::Kind::CONTROL;
            h.srcEp=h.dstEp=0;
            body[0]=outstanding_ ? 0x12 : 0x10; // DATA_ACK / TRANSPORT_ACK
            body[1]=ackSeq_;
            body[2]=ackStatus_;
            body[3]=static_cast<uint8_t>(port.freeSpace());
            body[4]=0;
            h.length=5;
            if (outstanding_) {
                body[5]=sendSeq_;
                for (uint8_t i=0; i<pendingLength_; ++i)
                    body[6+i]=pending_[i];
                h.length=static_cast<uint8_t>(6U+pendingLength_);
            }
        } else if (outstanding_) {
            h.kind=FrameV1::Kind::DATA;
            h.srcEp=h.dstEp=ENDPOINT;
            body[0]=sendSeq_;
            for (uint8_t i=0; i<pendingLength_; ++i)
                body[1+i]=pending_[i];
            h.length=static_cast<uint8_t>(pendingLength_+1U);
        } else {
            h.kind=FrameV1::Kind::DATA;
            h.length=0; // heartbeat if no UART data / ACK pending
        }
        return FrameV1::validate(h,body)==FrameV1::Error::OK;
    }

    // A TX slot was attempted. Hardware ACK is only a radio statistic.
    // The ACK pending flag may be cleared after TX_DS, because a duplicate
    // inbound DATA will solicit another ACK if the remote missed this one.
    void afterTransmit(bool hardwareAcked, bool carriedAck, bool carriedData) {
        if (carriedAck && hardwareAcked) ackPending_=false;
        if (carriedData && outstanding_) {
            if (txAttempts_ != 0 && stats_.retransmitted != 0xFFFF)
                ++stats_.retransmitted;
            if (txAttempts_ != 0xFF) ++txAttempts_;
        }
    }

    // Caller must verify FrameV1::decode, class addressing and session SID.
    // This function never commits partial UART output or interprets unknown
    // subtypes as acknowledgements.
    void accept(const FrameV1::View& frame, Port& port) {
        const FrameV1::Header& h=frame.header;
        if (h.kind==FrameV1::Kind::DATA) {
            if (h.length==0 && h.srcEp==0 && h.dstEp==0) return;
            if (h.srcEp!=ENDPOINT || h.dstEp!=ENDPOINT || h.length<2) return;
            acceptData(frame.body[0],frame.body+1,
                       static_cast<uint8_t>(h.length-1),port);
        } else if (h.kind==FrameV1::Kind::CONTROL && h.length>=5) {
            if (frame.body[0]==0x10 && h.length==5) {
                acceptAck(frame.body[1],frame.body[2]);
            } else if (frame.body[0]==0x12 && h.length>=7) {
                acceptAck(frame.body[1],frame.body[2]);
                acceptData(frame.body[5],frame.body+6,
                           static_cast<uint8_t>(h.length-6),port);
            }
        }
    }

private:
    void stage(Port& port) {
        pendingLength_=0;
        uint8_t value=0;
        while (pendingLength_<MAX_SEGMENT && port.read(value))
            pending_[pendingLength_++]=value;
        if (pendingLength_) {
            outstanding_=true;
            txAttempts_=0;
            ++stats_.segmentsQueued;
        }
    }

    void acceptAck(uint8_t seq,uint8_t status) {
        if (!outstanding_ || seq!=sendSeq_) return;
        if (status==ACCEPTED) {
            outstanding_=false;
            pendingLength_=0;
            sendSeq_=static_cast<uint8_t>(sendSeq_+1U);
            txAttempts_=0;
        } else if (status==BUSY) {
            ++stats_.ackBusy;
        } else {
            ++stats_.ackRejected; // preserve pending bytes, never silently drop
        }
    }

    void ack(uint8_t seq,uint8_t status) {
        ackPending_=true;
        ackSeq_=seq;
        ackStatus_=status;
    }

    void acceptData(uint8_t seq, const uint8_t* bytes,
                    uint8_t count, Port& port) {
        if (count==0 || count>23) { ++stats_.rxRejected; ack(seq,REJECTED); return; }
        if (seq==nextExpected_) {
            if (port.freeSpace()<count || !port.writeAll(bytes,count)) {
                ++stats_.rxBusy;
                ack(seq,BUSY);
                return;
            }
            ++stats_.segmentsAccepted;
            stats_.bytesAccepted=static_cast<uint16_t>(stats_.bytesAccepted+count);
            acceptedLast_=true;
            nextExpected_=static_cast<uint8_t>(nextExpected_+1U);
            ack(seq,ACCEPTED);
        } else if (acceptedLast_ &&
                   seq==static_cast<uint8_t>(nextExpected_-1U)) {
            ++stats_.duplicateRx;
            ack(seq,ACCEPTED);
        } else {
            ++stats_.rxRejected;
            ack(seq,REJECTED);
        }
    }

    uint8_t pending_[MAX_SEGMENT];
    bool outstanding_;
    uint8_t pendingLength_;
    uint8_t sendSeq_;
    uint8_t nextExpected_;
    bool acceptedLast_;
    bool ackPending_;
    uint8_t ackSeq_,ackStatus_,txAttempts_;
    Statistics stats_;
};
#endif
