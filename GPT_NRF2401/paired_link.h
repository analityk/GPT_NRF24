#ifndef GPT_NRF24_PAIRED_LINK_H
#define GPT_NRF24_PAIRED_LINK_H
#include <stdint.h>
#include "paired_frame.h"

// Pure state of one direction of a paired RF link. No UART, PHY or heap.
// Sequence and loss counters diagnose RF frames, NOT end-to-end delivery.
// PTX owns the observed ARC count; the PRX merely echoes last received ARC.
class PairedLink {
public:
    struct Counters {
        uint32_t rxFrames,gaps,duplicates,stale,foreign,rxDrops,txFrames;
        Counters():rxFrames(0),gaps(0),duplicates(0),stale(0),
          foreign(0),rxDrops(0),txFrames(0) {}
    };
    enum class Receipt : uint8_t { NEW_FRAME, DUPLICATE, STALE, WRONG_SESSION };
    explicit PairedLink(bool isA,uint16_t sid)
      : isA_(isA),sid_(sid),txSeq_(0),peerLast_(0),peerSeen_(false),
        localLoss_(0),remoteLoss_(0),localDrops_(0),arc_(0),peerCredit_(0) {}
    void resetSession(uint16_t sid) {
        sid_=sid;txSeq_=0;peerLast_=0;peerSeen_=false;
        localLoss_=0;remoteLoss_=0;localDrops_=0;arc_=0;peerCredit_=0;
        counters_=Counters();
    }
    uint16_t sessionId() const {return sid_;}
    uint8_t peerCredit() const {return peerCredit_;}
    uint8_t localLoss() const {return localLoss_;}
    uint8_t remoteLoss() const {return remoteLoss_;}
    uint8_t localDrops() const {return localDrops_;}
    const Counters& counters() const {return counters_;}
    void notePtxRetries(uint8_t arc) {
        if (isA_)arc_=static_cast<uint8_t>(arc>15U?15U:arc);
    }
    void noteLocalDrop() {
        localDrops_=static_cast<uint8_t>((localDrops_+1U)&15U);
        ++counters_.rxDrops;
    }
    PairedFrame::Error make(PairedFrame::Kind kind,
                 uint8_t extraFlags,uint8_t rxCredit,
                 const uint8_t* body,uint8_t length,
                 uint8_t* out,uint8_t& width) {
        PairedFrame::Header h;
        h.txSeq=txSeq_;
        h.rxLast=peerSeen_?peerLast_:0xFF;
        h.lossAB=isA_?remoteLoss_:localLoss_;
        h.lossBA=isA_?localLoss_:remoteLoss_;
        h.arcLast=arc_;
        h.rxDrop=localDrops_;
        h.rxCredit=rxCredit;
        h.flags=static_cast<uint8_t>((extraFlags&0xFCU)|
                                  static_cast<uint8_t>(kind));
        h.sessionId=sid_;
        const PairedFrame::Error e=PairedFrame::encode(h,body,length,out,width);
        if (e==PairedFrame::Error::OK){++txSeq_;++counters_.txFrames;}
        return e;
    }
    Receipt receive(const PairedFrame::View& f) {
        if(f.header.sessionId!=sid_) {
            ++counters_.foreign;
            return Receipt::WRONG_SESSION;
        }
        // Loss field provenance: B owns A->B; A owns B->A.
        remoteLoss_=isA_?f.header.lossAB:f.header.lossBA;
        peerCredit_=f.header.rxCredit;
        if(!isA_)arc_=f.header.arcLast; // echoed last known PTX ARC
        if(!peerSeen_){
            peerLast_=f.header.txSeq;
            peerSeen_=true;
            ++counters_.rxFrames;
            return Receipt::NEW_FRAME;
        }
        const uint8_t delta=static_cast<uint8_t>(f.header.txSeq-peerLast_);
        if(delta==0){++counters_.duplicates;return Receipt::DUPLICATE;}
        if(delta>127){++counters_.stale;return Receipt::STALE;}
        if(delta>1){
            const uint8_t missing=static_cast<uint8_t>(delta-1U);
            counters_.gaps+=missing;
            localLoss_=static_cast<uint8_t>((localLoss_+missing)&15U);
        }
        peerLast_=f.header.txSeq;
        ++counters_.rxFrames;
        return Receipt::NEW_FRAME;
    }
private:
    bool isA_;
    uint16_t sid_;
    uint8_t txSeq_,peerLast_;
    bool peerSeen_;
    uint8_t localLoss_,remoteLoss_,localDrops_,arc_,peerCredit_;
    Counters counters_;
};
#endif
