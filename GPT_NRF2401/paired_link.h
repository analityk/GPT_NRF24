#ifndef GPT_NRF24_PAIRED_LINK_H
#define GPT_NRF24_PAIRED_LINK_H
#include <stdint.h>
#include "paired_frame.h"

// Shared two-peer RF link state. No UART registers, MCU types or heap.
// Stream DATA seq identifies logical 24B user segments; IDLE does not
// advance sequence. RX_LAST is cumulative acceptance ACK (set by stream).
// A received RF packet is never equated with application acceptance.
// PTX owns OBSERVE_TX.ARC_CNT; PRX echoes its last reported value.
class PairedLink {
public:
    struct Counters {
        uint32_t rxFrames,gaps,duplicates,stale,foreign,rxDrops,txFrames;
        Counters():rxFrames(0),gaps(0),duplicates(0),stale(0),
          foreign(0),rxDrops(0),txFrames(0) {}
    };
    enum class Receipt : uint8_t { NEW_FRAME, DUPLICATE, STALE, WRONG_SESSION };
    explicit PairedLink(bool isA,uint16_t sid)
      : isA_(isA),sid_(sid),peerLast_(0),peerSeen_(false),
        localLoss_(0),remoteLoss_(0),localDrops_(0),arc_(0),peerCredit_(0) {}
    // Configure the same link-state object for either board role.
    // One instance per board suffices on the 2KB SRAM ATmega328P.
    void configure(bool isA,uint16_t sid) {
        isA_=isA;
        resetSession(sid);
    }
    void resetSession(uint16_t sid) {
        sid_=sid;peerLast_=0;peerSeen_=false;
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
    // Paired stream: MAC TX_SEQ is user DATA sequence and RX_LAST is
    // cumulative acceptance ACK. Both source types share the same header.
	PairedFrame::Header streamHeader(uint8_t seq,uint8_t ack,uint8_t credit,
		uint8_t length,bool missingPrefix) const{
		PairedFrame::Header h;
		h.txSeq=seq;
		h.rxLast=ack;
		h.lossAB=isA_?remoteLoss_:localLoss_;
		h.lossBA=isA_?localLoss_:remoteLoss_;
		h.arcLast=arc_;
		h.rxDrop=localDrops_;
		h.rxCredit=credit;
		h.flags=static_cast<uint8_t>(
			length!=0?PairedFrame::Kind::DATA:PairedFrame::Kind::IDLE);
		if(missingPrefix){
			h.flags=static_cast<uint8_t>(h.flags|PairedFrame::URGENT);
		}
		h.sessionId=sid_;
		return h;
	}

	PairedFrame::Error makeStream(uint8_t seq,uint8_t ack,uint8_t credit,
		const uint8_t* data,uint8_t length,uint8_t* out,uint8_t& width,
		bool missingPrefix=false){
		const PairedFrame::Header h=streamHeader(seq,ack,credit,length,
			missingPrefix);
		const PairedFrame::Error error=PairedFrame::encode(
			h,data,length,out,width);
		if(error==PairedFrame::Error::OK){
			++counters_.txFrames;
		}
		return error;
	}

	template<class Source>
	PairedFrame::Error makeStreamFrom(uint8_t seq,uint8_t ack,uint8_t credit,
		const Source& source,uint8_t first,uint8_t length,
		uint8_t* out,uint8_t& width,bool missingPrefix=false){
		const PairedFrame::Header h=streamHeader(seq,ack,credit,length,
			missingPrefix);
		const PairedFrame::Error error=PairedFrame::encodeFrom(
			h,source,first,length,out,width);
		if(error==PairedFrame::Error::OK){
			++counters_.txFrames;
		}
		return error;
	}

    // Repeated DATA may contain a newer ACK and credit. All valid frames
    // reach the stream, which owns user-level deduplication.
    Receipt observeStream(const PairedFrame::View& f) {
        if(f.header.sessionId!=sid_) {
            ++counters_.foreign;
            return Receipt::WRONG_SESSION;
        }
        ++counters_.rxFrames;
        acceptStatus(f);
        if(f.header.kind()!=PairedFrame::Kind::DATA || !f.length)
            return Receipt::NEW_FRAME;
        if(!peerSeen_) {
            peerLast_=f.header.txSeq;peerSeen_=true;
            return Receipt::NEW_FRAME;
        }
        const uint8_t delta=static_cast<uint8_t>(f.header.txSeq-peerLast_);
        if(delta==0){++counters_.duplicates;return Receipt::DUPLICATE;}
        if(delta>127U){++counters_.stale;return Receipt::STALE;}
        if(delta>1U) {
            const uint8_t missing=static_cast<uint8_t>(delta-1U);
            counters_.gaps+=missing;
            localLoss_=static_cast<uint8_t>((localLoss_+missing)&15U);
        }
        peerLast_=f.header.txSeq;
        return Receipt::NEW_FRAME;
    }

private:
    void acceptStatus(const PairedFrame::View& f) {
        // B owns A->B loss, A owns B->A loss; peer field is delayed echo.
        remoteLoss_=isA_?f.header.lossAB:f.header.lossBA;
        peerCredit_=f.header.rxCredit;
        if(!isA_)arc_=f.header.arcLast;
    }
    bool isA_;
    uint16_t sid_;
    uint8_t peerLast_;
    bool peerSeen_;
    uint8_t localLoss_,remoteLoss_,localDrops_,arc_,peerCredit_;
    Counters counters_;
};
#endif
