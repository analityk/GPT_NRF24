#ifndef GPT_NRF24_UART_RELIABLE_STREAM_H
#define GPT_NRF24_UART_RELIABLE_STREAM_H
#include <stdint.h>
#include "paired_link.h"

// Optional UART transport above the opaque 24-byte paired-link BODY.
// Two octets of transport control plus 0..22 arbitrary UART octets.
// Each direction has its OWN DATA sequence and application ACK, independent
// from the radio frame TX_SEQ/RX_LAST and from hardware ACK/ACK Payload.
//
// Stop-and-wait: at most one unacknowledged segment per direction. The
// outbound segment is retained across RF loss and session renegotiation,
// as long as THIS MCU remains powered. Sink acceptance is atomic via
// Port::writeAll, so an overflowing UART TX queue never acknowledges data.
// No heap, AVR headers, special byte values, logging, or project specifics.
//
// Port concept: read(uint8_t&), available(), freeSpace(),
//               writeAll(const uint8_t*,uint8_t).
template<class Port>
class UartReliableStream {
public:
    static const uint8_t CONTROL_BYTES=2;
    static const uint8_t MAX_USER_BYTES=PairedFrame::MAX_BODY-CONTROL_BYTES;

    explicit UartReliableStream(Port& port)
        : port_(port),session_(0),sendSeq_(0),nextReceive_(0),
          pending_(false),pendingLength_(0),
          acceptedBytes_(0),acknowledgedBytes_(0),
          duplicates_(0),blocked_(0),badFrames_(0) {}

    // Changes sequence-space epoch, but never discards bytes already read
    // from the USART RX queue into the pending segment.
    void beginSession(uint16_t sid) {
        if (session_==sid) return;
        session_=sid;
        sendSeq_=0;
        nextReceive_=0;
    }
    void disconnect() {beginSession(0);}

    bool hasPending() const {return pending_;}
    bool hasInput() const {return port_.available()!=0;}
    bool hasWork() const {return pending_ || hasInput();}
    uint16_t acceptedBytes() const {return acceptedBytes_;}
    uint16_t acknowledgedBytes() const {return acknowledgedBytes_;}
    uint16_t duplicates() const {return duplicates_;}
    uint16_t blocked() const {return blocked_;}
    uint16_t badFrames() const {return badFrames_;}

    // Receiver advertises full-size 24-byte reservation units. Actual
    // stream segments use <=22 bytes, so the credit is conservative.
    // Each incoming segment still passes the authoritative atomic writeAll
    // check: stale credits may never overcommit the actual UART TX queue.
    uint8_t credit() const {
        return static_cast<uint8_t>(port_.freeSpace()/
                                   PairedFrame::MAX_BODY);
    }

    // Every active radio exchange contains transport ACK. If idle, DATA
    // with two control octets (RF DPL width=10) replaces the old 8B IDLE.
    // A pending segment is retransmitted with its ORIGINAL data sequence,
    // even if a hardware ACK was received but app-level ACK was lost.
    bool make(PairedLink& link,uint8_t* dst,uint8_t& width) {
        if(session_==0 || link.sessionId()!=session_)return false;
        if(!pending_ && link.peerCredit()!=0)stageInput();
        uint8_t body[PairedFrame::MAX_BODY];
        body[0]=sendSeq_;
        body[1]=static_cast<uint8_t>(nextReceive_-1U);
        for(uint8_t i=0;i<pendingLength_ && pending_;++i)
            body[CONTROL_BYTES+i]=pendingBytes_[i];
        const uint8_t length=static_cast<uint8_t>(
            CONTROL_BYTES+(pending_?pendingLength_:0));
        return link.make(PairedFrame::Kind::DATA,0,credit(),body,length,
                         dst,width)==PairedFrame::Error::OK;
    }

    // Only call for a NEW, validated RF packet of the current session.
    // Return true if the peer attempted to send user data; useful for the
    // initiator's idle->fast polling decision.
    bool accept(const PairedFrame::View& frame) {
        if(session_==0 || frame.header.sessionId!=session_ ||
           frame.header.kind()!=PairedFrame::Kind::DATA)return false;
        if(frame.length<CONTROL_BYTES || !frame.body) {
            ++badFrames_;
            return false;
        }

        // ACK of bytes is completely independent of the RF ACK and of
        // RX_LAST (which reports radio reception, not USART acceptance).
        if(pending_ && frame.body[1]==sendSeq_) {
            acknowledgedBytes_=static_cast<uint16_t>(
                acknowledgedBytes_+pendingLength_);
            pending_=false;
            pendingLength_=0;
            ++sendSeq_;
        }

        const uint8_t count=static_cast<uint8_t>(
            frame.length-CONTROL_BYTES);
        if(count==0)return false;
        const uint8_t seq=frame.body[0];
        if(seq==nextReceive_) {
            if(port_.writeAll(frame.body+CONTROL_BYTES,count)) {
                ++nextReceive_;
                acceptedBytes_=static_cast<uint16_t>(acceptedBytes_+count);
            } else {
                // A retry is required; NEVER advertise ACK for this data.
                ++blocked_;
            }
        } else if(seq==static_cast<uint8_t>(nextReceive_-1U)) {
            ++duplicates_;
        } else {
            // Stop-and-wait prohibits future data; neither acknowledge
            // nor mutate any UART state for stale/out-of-order segments.
            ++badFrames_;
        }
        return true;
    }
private:
    void stageInput() {
        uint8_t value=0;
        pendingLength_=0;
        while(pendingLength_<MAX_USER_BYTES && port_.read(value))
            pendingBytes_[pendingLength_++]=value;
        pending_=(pendingLength_!=0);
    }

    Port& port_;
    uint16_t session_;
    uint8_t sendSeq_,nextReceive_;
    bool pending_;
    uint8_t pendingLength_;
    uint8_t pendingBytes_[MAX_USER_BYTES];
    uint16_t acceptedBytes_,acknowledgedBytes_;
    uint16_t duplicates_,blocked_,badFrames_;
};
#endif
