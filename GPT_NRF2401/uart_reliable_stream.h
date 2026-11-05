#ifndef GPT_NRF24_UART_RELIABLE_STREAM_H
#define GPT_NRF24_UART_RELIABLE_STREAM_H
#include <stdint.h>
#include "paired_link.h"

// Optional phase callbacks; the ordinary transport API compiles them away.
struct UartMakeNoopTrace {
    void sourceReady()const{}
    void frameEncoded()const{}
};

// Full-rate bidirectional UART stream over opaque 24-byte RF BODY.
// DATA_SEQ and cumulative APP_ACK occupy MAC TX_SEQ/RX_LAST.
// All 24 BODY octets are user bytes, without transport preamble.
// Four unacknowledged segments per direction: new data can be emitted on
// successive RF exchanges while older segments await delayed ACK Payload.
// Go-back-N on ACK timeout (in EXCHANGES, not milliseconds), no heap.
// Received data is committed only in order, atomically by Port::writeAll.
// After a gap/timeout/rekey, drain the unacknowledged window first:
// IDLE still carries ACK/credit, but fresh DATA is not admitted until all
// previously cached segments are confirmed. No radio timing adjustment.
// All state is independent of AVR, UART registers, and radio implementation.
template<class Port>
class UartReliableStream {
public:
    static const uint8_t MAX_USER_BYTES=PairedFrame::MAX_BODY;
    static const uint8_t WINDOW=4;
    static const uint8_t REPLAY_AFTER=8; // multiple queued RF/ACK turns

    explicit UartReliableStream(Port& port)
       :port_(port),session_(0),baseSeq_(0),nextSeq_(0),headIndex_(0),
        recvNext_(0),outstanding_(0),replayCount_(0),replaySeq_(0),
        noAckTurns_(0),gapPending_(false),nackSeen_(false),nackForBase_(0),
        draining_(false),
        acceptedBytes_(0),acknowledgedBytes_(0),
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
        gapPending_=false;
        nackSeen_=false;
        replayCount_=outstanding_;
        replaySeq_=0;
        draining_=(outstanding_!=0);
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

    // A frozen link still needs IDLE probes carrying the last accepted ACK,
    // but MUST NOT drain host input or replay stale DATA during RF silence.
    bool make(PairedLink& link,uint8_t* dst,uint8_t& width,
              bool permitData=true) {
        UartMakeNoopTrace trace;
        return makeTraced(link,dst,width,permitData,trace);
    }

    // Optional portable observer for profiling source/cache work separately
    // from final MAC serialization. It neither reads a clock itself nor
    // changes stream state, so mock clocks can test both boundaries.
    template<class Trace>
    bool makeTraced(PairedLink& link,uint8_t* dst,uint8_t& width,
                    bool permitData,Trace& trace) {
        if(session_==0||link.sessionId()!=session_)return false;
        uint8_t body[PairedFrame::MAX_BODY];
        uint8_t seq=nextSeq_;
        uint8_t count=0;
        const uint8_t peerCredit=link.peerCredit();

        // Peer credit measures *currently free receiver storage*.  A
        // replay needs that storage as much as a first transmission does.
        // While credit is zero, keep the cache and send only IDLE/ACK:
        // retrying a blocked segment cannot make the sink accept it and
        // may cause an endless out-of-order/retransmit feedback loop.
        const bool maySend=permitData && peerCredit!=0;
        if(maySend && outstanding_!=0 && noAckTurns_<255)++noAckTurns_;
        if(maySend && outstanding_!=0 && replayCount_==0 &&
           noAckTurns_>=REPLAY_AFTER) {
            replaySeq_=baseSeq_;
            replayCount_=outstanding_;
            noAckTurns_=0;
            draining_=true;
        }

        if(maySend && replayCount_!=0) {
            seq=replaySeq_;
            count=slotLength_[slotIndex(seq)];
            for(uint8_t i=0;i<count;++i)
                body[i]=slots_[slotIndex(seq)][i];
            ++replaySeq_;
            --replayCount_;
            ++retransmits_;
        } else if(maySend && !draining_ && outstanding_<WINDOW &&
                  outstanding_<peerCredit &&
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
                    body[i]=slots_[slotIndex(seq)][i];
            }
        }
        trace.sourceReady();
        const PairedFrame::Error encoded=link.makeStream(
            seq,static_cast<uint8_t>(recvNext_-1U),
            credit(),body,count,dst,width,gapPending_);
        trace.frameEncoded();
        return encoded==PairedFrame::Error::OK;
    }

    // Return whether a nonempty user segment was received. This is NOT
    // proof of success: a full sink may refuse it and leave ACK unchanged.
    bool accept(const PairedFrame::View& f) {
        if(session_==0 || f.header.sessionId!=session_ ||
           (f.header.kind()!=PairedFrame::Kind::DATA &&
            f.header.kind()!=PairedFrame::Kind::IDLE))return false;
        // Every frame, including IDLE and retransmitted DATA, conveys ACK.
        acceptAck(f.header.rxLast);
        // Go-back-N receiver reports a hole via an already allocated MAC
        // flag. Rewind immediately only ONCE per missing base sequence:
        // queued old ACK payloads otherwise cause a replay storm.
        if((f.header.flags&PairedFrame::URGENT)!=0 && outstanding_!=0 &&
           static_cast<uint8_t>(f.header.rxLast+1U)==baseSeq_ &&
           (!nackSeen_ || nackForBase_!=baseSeq_)) {
            replaySeq_=baseSeq_;
            replayCount_=outstanding_;
            noAckTurns_=0;
            nackForBase_=baseSeq_;
            nackSeen_=true;
            draining_=true;
        }
        if(f.header.kind()==PairedFrame::Kind::IDLE)return false;
        if(!f.length || !f.body) {
            ++badFrames_;
            return false;
        }
        const uint8_t n=f.length;
        const uint8_t seq=f.header.txSeq;
        if(seq==recvNext_) {
            if(port_.writeAll(f.body,n)) {
                ++recvNext_;
                gapPending_=false;
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
            gapPending_=true;
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
        if(outstanding_==0)draining_=false;
        noAckTurns_=0;
        nackSeen_=false;
        // ACK progress must NOT cancel the yet-unsent tail of a replay.
        // In particular after a peer reset, replay 0..3 may be queued in
        // ACK Payload while the new peer already ACKs 0. Dropping replay
        // 1..3 here would emit fresh DATA 4 into a hole and lock the
        // receiver into a repeating out-of-order/replay cycle.
        if(replayCount_!=0) {
            const uint8_t delta=static_cast<uint8_t>(baseSeq_-replaySeq_);
            if(delta<=WINDOW) {
                const uint8_t covered=delta<replayCount_?delta:replayCount_;
                replaySeq_=static_cast<uint8_t>(replaySeq_+covered);
                replayCount_=static_cast<uint8_t>(replayCount_-covered);
            }
        }
        if(replayCount_==0)replaySeq_=baseSeq_;
    }
    Port& port_;
    uint16_t session_;
    uint8_t baseSeq_,nextSeq_,headIndex_,recvNext_,outstanding_;
    uint8_t replayCount_,replaySeq_,noAckTurns_;
    bool gapPending_,nackSeen_;
    uint8_t nackForBase_;
    bool draining_;
    uint8_t slots_[WINDOW][MAX_USER_BYTES];
    uint8_t slotLength_[WINDOW];
    uint16_t acceptedBytes_,acknowledgedBytes_;
    uint16_t duplicates_,blocked_,badFrames_,retransmits_;
};
#endif
