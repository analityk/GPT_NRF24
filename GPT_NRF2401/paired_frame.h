#ifndef GPT_NRF24_PAIRED_FRAME_H
#define GPT_NRF24_PAIRED_FRAME_H
#include <stdint.h>

// Paired two-node profile. Binary header offsets are explicit: no packed ABI.
// This is a NEW negotiated profile; it must not be decoded as legacy FrameV1.
namespace PairedFrame {
    static const uint8_t HEADER_SIZE=8;
    static const uint8_t MAX_BODY=24;
    static const uint8_t MAX_FRAME=32;
    enum class Kind : uint8_t { DATA=0, IDLE=1, CONTROL=2, EXT=3 };
    enum class Error : uint8_t { OK, NULL_BUFFER, BAD_LENGTH, BAD_TYPE };
    enum Flag : uint8_t {
        // On DATA/IDLE: receiver requests fast Go-back-N replay starting
        // at RX_LAST+1. No extra packet bytes or transport payload header.
        URGENT=0x04, PAUSE=0x08, RESYNC=0x10,
        WANT_PTX=0x20, OVERFLOW=0x40, REPORT=0x80
    };
    struct Header {
        uint8_t txSeq,rxLast,lossAB,lossBA,arcLast,rxDrop,rxCredit,flags;
        uint16_t sessionId;
        Header():txSeq(0),rxLast(0xFF),lossAB(0),lossBA(0),
            arcLast(0),rxDrop(0),rxCredit(0),flags(0),sessionId(0) {}
        Kind kind() const {return static_cast<Kind>(flags&0x03U);}
    };
    struct View {
        Header header;
        const uint8_t* body;
        uint8_t length;
        View():body(0),length(0) {}
    };
    inline Error validate(const Header& h,uint8_t length) {
        if (length>MAX_BODY) return Error::BAD_LENGTH;
        if ((h.lossAB|h.lossBA|h.arcLast|h.rxDrop)&0xF0U)
            return Error::BAD_TYPE;
        if (h.kind()==Kind::IDLE && length!=0) return Error::BAD_TYPE;
        if (h.kind()==Kind::CONTROL && length==0) return Error::BAD_TYPE;
        return Error::OK;
    }
    // Header offsets are the on-air protocol layout, not an AVR ABI.
#define PAIRED_FRAME_TX_SEQ_OFFSET 0U
#define PAIRED_FRAME_RX_LAST_OFFSET 1U
#define PAIRED_FRAME_LOSS_OFFSET 2U
#define PAIRED_FRAME_DROP_RETRY_OFFSET 3U
#define PAIRED_FRAME_CREDIT_OFFSET 4U
#define PAIRED_FRAME_FLAGS_OFFSET 5U
#define PAIRED_FRAME_SID_LOW_OFFSET 6U
#define PAIRED_FRAME_SID_HIGH_OFFSET 7U
#define PAIRED_FRAME_NIBBLE_SHIFT 4U

	inline void encodeHeader(const Header& h,uint8_t* dst){
		dst[PAIRED_FRAME_TX_SEQ_OFFSET]=h.txSeq;
		dst[PAIRED_FRAME_RX_LAST_OFFSET]=h.rxLast;
		dst[PAIRED_FRAME_LOSS_OFFSET]=static_cast<uint8_t>(
			(h.lossBA<<PAIRED_FRAME_NIBBLE_SHIFT)|h.lossAB);
		dst[PAIRED_FRAME_DROP_RETRY_OFFSET]=static_cast<uint8_t>(
			(h.rxDrop<<PAIRED_FRAME_NIBBLE_SHIFT)|h.arcLast);
		dst[PAIRED_FRAME_CREDIT_OFFSET]=h.rxCredit;
		dst[PAIRED_FRAME_FLAGS_OFFSET]=h.flags;
		dst[PAIRED_FRAME_SID_LOW_OFFSET]=static_cast<uint8_t>(h.sessionId);
		dst[PAIRED_FRAME_SID_HIGH_OFFSET]=static_cast<uint8_t>(
			h.sessionId>>8U);
	}

	inline Error encode(const Header& h,const uint8_t* body,uint8_t length,
		uint8_t* dst,uint8_t& width){
		if(!dst || (length!=0 && !body)){
			return Error::NULL_BUFFER;
		}
		const Error error=validate(h,length);
		if(error!=Error::OK){
			return error;
		}
		encodeHeader(h,dst);
		for(uint8_t i=0;i<length;++i){
			dst[HEADER_SIZE+i]=body[i];
		}
		width=static_cast<uint8_t>(HEADER_SIZE+length);
		return Error::OK;
	}

	// Serialize from a retained ring straight into the one radio frame.
	// The source may wrap at 255 -> 0; no staging BODY array is required.
	template<class Source>
	inline Error encodeFrom(const Header& h,const Source& source,uint8_t first,
		uint8_t length,uint8_t* dst,uint8_t& width){
		if(!dst){
			return Error::NULL_BUFFER;
		}
		const Error error=validate(h,length);
		if(error!=Error::OK){
			return error;
		}
		encodeHeader(h,dst);
		for(uint8_t i=0;i<length;++i){
			dst[HEADER_SIZE+i]=source.at(static_cast<uint8_t>(first+i));
		}
		width=static_cast<uint8_t>(HEADER_SIZE+length);
		return Error::OK;
	}

    // RF dynamic width is the ONLY length source; do not require zero padding.
    inline Error decode(const uint8_t* src,uint8_t width,View& dst) {
        if (!src) return Error::NULL_BUFFER;
        if (width<HEADER_SIZE || width>MAX_FRAME) return Error::BAD_LENGTH;
        Header h;
        h.txSeq=src[0];h.rxLast=src[1];
        h.lossAB=static_cast<uint8_t>(src[2]&0x0FU);
        h.lossBA=static_cast<uint8_t>(src[2]>>4);
        h.arcLast=static_cast<uint8_t>(src[3]&0x0FU);
        h.rxDrop=static_cast<uint8_t>(src[3]>>4);
        h.rxCredit=src[4];h.flags=src[5];
        h.sessionId=static_cast<uint16_t>(src[6]|
            (static_cast<uint16_t>(src[7])<<8));
        const uint8_t length=static_cast<uint8_t>(width-HEADER_SIZE);
        const Error e=validate(h,length);
        if (e!=Error::OK)return e;
        dst.header=h;dst.body=src+HEADER_SIZE;dst.length=length;
        return Error::OK;
    }
}
#endif
