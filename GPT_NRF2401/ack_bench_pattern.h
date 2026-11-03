#ifndef GPT_NRF24_ACK_BENCH_PATTERN_H
#define GPT_NRF24_ACK_BENCH_PATTERN_H
#include <stdint.h>

// Pure 32-byte integrity test. This is NOT the application v1 protocol;
// full 32-byte bidirectional RF payloads are tested in isolation.
namespace AckBenchPattern {
    static const uint8_t SIZE=32;
    inline void make(uint8_t origin,uint32_t seq,uint8_t (&out)[SIZE]) {
        out[0]=0xD2;out[1]=0x74;
        out[2]=origin;out[3]=0x01;
        for (uint8_t i=0;i<4;++i)
            out[4+i]=static_cast<uint8_t>(seq>>(i*8));
        for (uint8_t i=8;i<SIZE;++i)
            out[i]=static_cast<uint8_t>(
                (seq*17UL + static_cast<uint32_t>(i)*29UL +
                 static_cast<uint32_t>(origin)*53UL) ^
                (seq>>((i&3U)*8U)));
    }
    inline bool inspect(const uint8_t* frame,uint8_t origin,uint32_t& seq) {
        if (!frame || frame[0]!=0xD2 || frame[1]!=0x74 ||
            frame[2]!=origin || frame[3]!=1) return false;
        seq=0;
        for (uint8_t i=0;i<4;++i)
            seq|=static_cast<uint32_t>(frame[4+i])<<(i*8);
        uint8_t expected[SIZE];
        make(origin,seq,expected);
        for (uint8_t i=0;i<SIZE;++i)
            if (frame[i]!=expected[i]) return false;
        return true;
    }

    // Uses modulo-2^32 sequence distances. Gap count is diagnostic only,
    // not an ARQ guarantee. Handles natural wrap of the test counter.
    class Tracker {
    public:
        Tracker():seen_(false),last_(0),gaps_(0),duplicates_(0) {}
        void observe(uint32_t seq) {
            if (!seen_) { seen_=true;last_=seq;return; }
            const uint32_t delta=seq-last_;
            if (delta==0 || delta>0x7FFFFFFFUL) {
                ++duplicates_;
            } else {
                if (delta>1) gaps_+=delta-1;
                last_=seq;
            }
        }
        uint32_t gaps() const {return gaps_;}
        uint32_t duplicates() const {return duplicates_;}
    private:
        bool seen_;
        uint32_t last_,gaps_,duplicates_;
    };
}
#endif
