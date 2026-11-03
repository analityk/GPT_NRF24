#ifndef GPT_NRF24_RAW_PATTERN_H
#define GPT_NRF24_RAW_PATTERN_H

#include <stdint.h>

// Minimal diagnostic fixture, NOT the network protocol. Only 32 raw RF bytes.
// Both bytes of sequence contribute to every pattern byte, so single-byte
// corruption and stale replies are detectable independently of radio CRC.
namespace RawRfPattern {
    static const uint8_t REQUEST=0xA5;
    static const uint8_t REPLY=0x5A;

    inline void make(uint8_t* frame, uint16_t sequence) {
        frame[0]=REQUEST;
        frame[1]=static_cast<uint8_t>(sequence);
        frame[2]=static_cast<uint8_t>(sequence >> 8);
        const uint8_t seed=static_cast<uint8_t>(sequence ^ (sequence >> 8));
        for (uint8_t i=3; i<32; ++i)
            frame[i]=static_cast<uint8_t>(seed + (uint16_t)i * 17U);
    }

    inline bool validate(const uint8_t* frame, uint8_t marker, uint16_t& sequence) {
        if (frame[0]!=marker) return false;
        sequence=static_cast<uint16_t>(
            frame[1] | (static_cast<uint16_t>(frame[2])<<8));
        const uint8_t seed=static_cast<uint8_t>(sequence ^ (sequence >> 8));
        for (uint8_t i=3; i<32; ++i) {
            if (frame[i]!=static_cast<uint8_t>(seed + (uint16_t)i * 17U))
                return false;
        }
        return true;
    }

    inline void markReply(uint8_t* frame) { frame[0]=REPLY; }
}
#endif
