#ifndef GPT_NRF24_V1_PROBE_PATTERN_H
#define GPT_NRF24_V1_PROBE_PATTERN_H

#include <stdint.h>
#include "frame_v1.h"

// Temporary payload bring-up service (endpoint 2), NOT reliable UART.
// It is intentionally separate from the generic frame encoder and scheduler.
namespace V1ProbePattern {
    static const uint8_t ENDPOINT=2;
    static const uint8_t TEST_SID=0x2A; // explicitly NOT a negotiated session

    inline uint8_t length(uint8_t seq, uint8_t origin) {
        switch (static_cast<uint8_t>((seq + origin*2U) & 3U)) {
            case 0: return 0;
            case 1: return 1;
            case 2: return 23;
            default: return 24;
        }
    }
    inline uint8_t octet(uint8_t seq, uint8_t origin, uint8_t i) {
        return static_cast<uint8_t>((seq * 17U) ^ (i * 29U) ^ (origin * 0xA5U));
    }
    inline FrameV1::Error make(uint8_t src, uint8_t dst, uint8_t seq,
                                uint8_t lastRx, uint8_t* out) {
        FrameV1::Header h;
        h.kind=FrameV1::Kind::DATA;
        h.src=src; h.dst=dst; h.sid=TEST_SID;
        h.txSeq=seq; h.rxLast=lastRx;
        h.length=length(seq,src);
        if (h.length != 0) h.srcEp=h.dstEp=ENDPOINT;
        uint8_t bytes[FrameV1::BODY_SIZE];
        for (uint8_t i=0; i<h.length; ++i) bytes[i]=octet(seq,src,i);
        return FrameV1::encode(h, bytes, out);
    }
    inline bool inspect(const FrameV1::View& v, uint8_t sender,
                        uint8_t recipient) {
        const FrameV1::Header& h=v.header;
        if (h.kind != FrameV1::Kind::DATA || h.src!=sender ||
            h.dst!=recipient || h.sid!=TEST_SID) return false;
        if (h.length != length(h.txSeq,sender)) return false;
        if (h.length == 0) {
            return h.srcEp==0 && h.dstEp==0;
        }
        if (h.srcEp!=ENDPOINT || h.dstEp!=ENDPOINT || !v.body)
            return false;
        for (uint8_t i=0; i<h.length; ++i)
            if (v.body[i]!=octet(h.txSeq,sender,i)) return false;
        return true;
    }
}
#endif
