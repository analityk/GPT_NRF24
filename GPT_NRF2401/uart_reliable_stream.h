#ifndef GPT_NRF24_UART_RELIABLE_STREAM_H
#define GPT_NRF24_UART_RELIABLE_STREAM_H
#include <stdint.h>
#include "paired_link.h"

// Full-rate bidirectional UART stream over opaque 24-byte RF BODY.
// 2B metadata (DATA_SEQ + cumulative APP_ACK) and up to 22B raw bytes.
// Four unacknowledged segments per direction: new data can be emitted on
// successive RF exchanges while older segments await delayed ACK Payload.
// Go-back-N on ACK timeout (in EXCHANGES, not milliseconds), no heap.
// Received data is committed only in order, atomically by Port::writeAll.
// All state is independent of AVR, UART registers, and radio implementation.
template<class Port>
class UartReliableStream {
public:
    static const uint8_t CONTROL_BYTES=2;
    static const uint8_t MAX_USER_BYTES=PairedFrame::MAX_BODY-CONTROL_BYTES;
    static const uint8_t WINDOW=4;
    static const uint8_t REPLAY_AFTER=8; // multiple queued RF/ACK turns

    explicit UartReliableStream(Port& port)
       :port_(port),session_(0),baseSeq_(0),nextSeq_(0),headIndex_(0),
        recvNext_(0),outstanding_(0),replayCount_(0),replaySeq_(0),
        noAckTurns_(0),acceptedBytes_(0),acknowledgedBytes_(0),
        duplicates_(0),blocked_(0),badFrames_(0),retransmits_(0) {
        for(uint8_t i=0;i<WINDOW;++i)slotLength_[i]=0;
    }

    // A new RF session starts a new 8-bit stream numbering epoch.
    // Keep ALL unacknowledged segments in their original byte order;
    // a surviving sender can retransmit them across peer reset.
    void beginSession(uint16_t sid) {
        if(session_==sid)return;
        session_=sid;
        baseSeq_=0;
        nextSeq_=outstanding_;
        recvNext_=0;
        noAckTurns_=0;
        replayCount_=outstanding_;
        replaySeq_=0;
        // Physical data already accepted in an old epoch may be delivered
        // again if its final ACK was lost. Exactly-once across reset needs
        // a separate durable protocol, not this volatile UART transport.
    }
    void disconnect(){beginSession(0);}
    bool hasPending()const{return outstanding_!=0;}
    bool hasInput()const{return port_.available()!=0;}
    bool hasWork()const{return hasPending()||hasInput();}
    uint16_t acceptedBytes()const{return acceptedBytes_;}
    uint16_t acknowledgedBytes()const{return acknowledgedBytes_;}
    uint16_t duplicates()const{return duplicates_;}
    uint16_t blocked()const{return blocked_;}
    uint16_t badFrames()const{return badFrames_;}
    uint16_t retransmits()const{return retransmits_;}
    uint8_t outstanding()const{return outstanding_;}

    // Advertise actual sink headroom in conservative 24B units.
    // Incoming segments are still checked by an atomic UART writeAll.
    uint8_t credit()const{
        return static_cast<uint8_t>(port_.freeSpace()/
                                     PairedFrame::MAX_BODY);
    }

    bool make(PairedLink& link,uint8_t* dst,uint8_t& width) {
        if(session_==0||link.sessionId()!=session_)return false;
        uint8_t body[PairedFrame::MAX_BODY];
        uint8_t seq=nextSeq_;
        uint8_t count=0;
        const uint8_t peerCredit=link.peerCredit();

        if(outstanding_!=0 && noAckTurns_<255)++noAckTurns_;
        if(outstanding_!=0 && replayCount_==0 &&
           noAckTurns_>=REPLAY_AFTER) {
            replaySeq_=baseSeq_;
            replayCount_=outstanding_;
            noAckTurns_=0;
        }

        if(replayCount_!=0) {
            seq=replaySeq_;
            count=slotLength_[slotIndex(seq)];
            for(uint8_t i=0;i<count;++i)
                body[CONTROL_BYTES+i]=slots_[slotIndex(seq)][i];
            ++replaySeq_;
            --replayCount_;
            ++retransmits_;
        } else if(outstanding_<WINDOW && outstanding_<peerCredit &&
                  port_.available()!=0) {
            seq=nextSeq_;
            uint8_t value=0;
            while(count<MAX_USER_BYTES && port_.read(value))
                slots_[slotIndex(seq)][count++]=value;
            if(count) {
                slotLength_[slotIndex(seq)]=count;
                ++nextSeq_;
                ++outstanding_;
                for(uint8_t i=0;i<count;++i)
                    body[CONTROL_BYTES+i]=slots_[slotIndex(seq)][i];
            }
        }
        body[0]=seq;
        body[1]=static_cast<uint8_t>(recvNext_-1U);
        return link.make(PairedFrame::Kind::DATA,0,credit(),body,
                         static_cast<uint8_t>(CONTROL_BYTES+count),
                         dst,width)==PairedFrame::Error::OK;
    }

    // Return whether a nonempty user segment was received. This is NOT
    // proof of success: a full sink may refuse it and leave ACK unchanged.
    bool accept(const PairedFrame::View& f) {
        if(session_==0 || f.header.sessionId!=session_ ||
           f.header.kind()!=PairedFrame::Kind::DATA)return false;
        if(f.length<CONTROL_BYTES || !f.body) {
            ++badFrames_;
            return false;
        }
        acceptAck(f.body[1]);

        const uint8_t n=static_cast<uint8_t>(f.length-CONTROL_BYTES);
        if(!n)return false;
        const uint8_t seq=f.body[0];
        if(seq==recvNext_) {
            if(port_.writeAll(f.body+CONTROL_BYTES,n)) {
                ++recvNext_;
                acceptedBytes_=static_cast<uint16_t>(acceptedBytes_+n);
            } else {
                ++blocked_; // NEVER ACK undelivered user bytes
            }
        } else if(static_cast<uint8_t>(recvNext_-seq)<=WINDOW) {
            // A duplicate inside our bounded sender window.
            ++duplicates_;
        } else {
            // Out-of-order future frame: Go-back-N receiver. Sender must
            // replay the missing oldest segment before advancement.
            ++badFrames_;
        }
        return true;
    }

private:
    // Physical slots follow the oldest unacknowledged DATA, not the
    // current 8-bit wire sequence. This preserves all pending bytes on
    // session reset even when the old sendBase had wrapped or advanced.
    uint8_t slotIndex(uint8_t seq) const {
        const uint8_t offset=static_cast<uint8_t>(seq-baseSeq_);
        return static_cast<uint8_t>((headIndex_+offset)%WINDOW);
    }
    void acceptAck(uint8_t ack) {
        if(!outstanding_)return;
        // Modulo arithmetic valid with WINDOW<128 and non-aliased epochs.
        const uint8_t distance=static_cast<uint8_t>(ack-baseSeq_);
        if(distance>=outstanding_)return; // stale/future/unknown ACK
        const uint8_t n=static_cast<uint8_t>(distance+1U);
        for(uint8_t i=0;i<n;++i) {
            const uint8_t slot=slotIndex(static_cast<uint8_t>(baseSeq_+i));
            acknowledgedBytes_=static_cast<uint16_t>(
                acknowledgedBytes_+slotLength_[slot]);
            slotLength_[slot]=0;
        }
        headIndex_=static_cast<uint8_t>((headIndex_+n)%WINDOW);
        baseSeq_=static_cast<uint8_t>(baseSeq_+n);
        outstanding_=static_cast<uint8_t>(outstanding_-n);
        noAckTurns_=0;
        // The peer has advanced its contiguous prefix. Any outstanding
        // gap is handled by normal retransmission on the next timeout.
        replayCount_=0;
        replaySeq_=baseSeq_;
    }
    Port& port_;
    uint16_t session_;
    uint8_t baseSeq_,nextSeq_,headIndex_,recvNext_,outstanding_;
    uint8_t replayCount_,replaySeq_,noAckTurns_;
    uint8_t slots_[WINDOW][MAX_USER_BYTES];
    uint8_t slotLength_[WINDOW];
    uint16_t acceptedBytes_,acknowledgedBytes_;
    uint16_t duplicates_,blocked_,badFrames_,retransmits_;
};
#endif
