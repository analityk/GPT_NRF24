#ifndef GPT_NRF24_BURST20_REPORT_H
#define GPT_NRF24_BURST20_REPORT_H
#include <stdint.h>
#include "burst20_schedule.h"

// End-of-slot 32B control frame: 8B PairedFrame header + exactly 24B BODY.
// The codec is independent of SPI, ATmega EEPROM, clocks or UART.
// A received bitmap acknowledges a PREVIOUS block from the opposite owner,
// never the just-completed block that this sender has not yet heard about.
namespace Burst20Report {
    static const uint8_t VERSION=1;
    static const uint8_t BODY_SIZE=24;
    static const uint8_t MAX_REPORTED_PACKETS=32;
    enum Flag : uint8_t {
        OVERFLOW=0x01, RESYNC=0x02, MISSED_REPORT=0x04,
        PEER_PAUSED=0x08, CRC_MISMATCH=0x10
    };

    struct Summary {
        uint16_t cycle;
        uint16_t peerCycle;
        uint32_t dataCrc32;
        uint32_t peerBitmap;
        uint16_t dataBytes;
        uint8_t firstSeq;
        uint8_t dataPackets;
        uint8_t acceptedPeerPackets;
        uint8_t missingPeerPackets;
        uint8_t peerCredit;
        uint8_t flags;
        Burst20Schedule::Share share;
        Burst20Schedule::Owner owner;

        Summary():cycle(0),peerCycle(0),dataCrc32(0),peerBitmap(0),
            dataBytes(0),firstSeq(0),dataPackets(0),
            acceptedPeerPackets(0),missingPeerPackets(0),
            peerCredit(0),flags(0),share(Burst20Schedule::Share::A10_B10),
            owner(Burst20Schedule::Owner::A){}
    };
    inline void put16(uint8_t* dst,uint16_t value) {
        dst[0]=static_cast<uint8_t>(value);
        dst[1]=static_cast<uint8_t>(value>>8);
    }
    inline void put32(uint8_t* dst,uint32_t value) {
        for(uint8_t i=0;i<4;++i)
            dst[i]=static_cast<uint8_t>(value>>(8U*i));
    }
    inline uint16_t get16(const uint8_t* src) {
        return static_cast<uint16_t>(
            static_cast<uint16_t>(src[0]) |
            (static_cast<uint16_t>(src[1])<<8));
    }
    inline uint32_t get32(const uint8_t* src) {
        uint32_t value=0;
        for(uint8_t i=0;i<4;++i)
            value|=static_cast<uint32_t>(src[i])<<(8U*i);
        return value;
    }

    inline bool valid(const Summary& v) {
        return Burst20Schedule::valid(v.share) &&
            (v.owner==Burst20Schedule::Owner::A ||
             v.owner==Burst20Schedule::Owner::B) &&
            v.dataPackets<=MAX_REPORTED_PACKETS &&
            v.acceptedPeerPackets<=MAX_REPORTED_PACKETS &&
            v.missingPeerPackets<=MAX_REPORTED_PACKETS &&
            static_cast<uint16_t>(v.acceptedPeerPackets+
                v.missingPeerPackets)<=MAX_REPORTED_PACKETS &&
            v.dataBytes<=static_cast<uint16_t>(
                v.dataPackets*24U) &&
            (v.flags&static_cast<uint8_t>(~0x1FU))==0;
    }

    inline bool encode(const Summary& v,uint8_t* dst,uint8_t width) {
        if(!dst || width<BODY_SIZE || !valid(v))return false;
        uint8_t bytes[BODY_SIZE]={};
        bytes[0]=VERSION;
        bytes[1]=static_cast<uint8_t>(v.share);
        bytes[2]=static_cast<uint8_t>(v.owner);
        bytes[3]=v.flags;
        put16(bytes+4,v.cycle);
        bytes[6]=v.firstSeq;
        bytes[7]=v.dataPackets;
        put16(bytes+8,v.dataBytes);
        put32(bytes+10,v.dataCrc32);
        put16(bytes+14,v.peerCycle);
        put32(bytes+16,v.peerBitmap);
        bytes[20]=v.acceptedPeerPackets;
        bytes[21]=v.missingPeerPackets;
        bytes[22]=v.peerCredit;
        bytes[23]=0; // reserved; v1 requires zero
        for(uint8_t i=0;i<BODY_SIZE;++i)dst[i]=bytes[i];
        return true;
    }
    inline bool decode(const uint8_t* src,uint8_t width,Summary& dst) {
        if(!src || width!=BODY_SIZE || src[0]!=VERSION ||
           src[23]!=0)return false;
        Summary tmp;
        tmp.share=static_cast<Burst20Schedule::Share>(src[1]);
        tmp.owner=static_cast<Burst20Schedule::Owner>(src[2]);
        tmp.flags=src[3];
        tmp.cycle=get16(src+4);
        tmp.firstSeq=src[6];tmp.dataPackets=src[7];
        tmp.dataBytes=get16(src+8);
        tmp.dataCrc32=get32(src+10);
        tmp.peerCycle=get16(src+14);
        tmp.peerBitmap=get32(src+16);
        tmp.acceptedPeerPackets=src[20];
        tmp.missingPeerPackets=src[21];
        tmp.peerCredit=src[22];
        if(!valid(tmp))return false;
        dst=tmp;
        return true;
    }

    // CRC-32/ISO-HDLC: reflected poly 0xEDB88320, init and xorout 0xFFFFFFFF.
    // Incremental state fits in 4B, so no 1024B block buffer on the MCU.
    class RollingCrc32 {
    public:
        RollingCrc32():state_(0xFFFFFFFFUL){}
        void reset(){state_=0xFFFFFFFFUL;}
        void add(uint8_t byte) {
            state_^=byte;
            for(uint8_t bit=0;bit<8;++bit)
                state_=(state_&1U)?(state_>>1)^0xEDB88320UL:(state_>>1);
        }
        void add(const uint8_t* data,uint8_t length) {
            if(!data)return;
            for(uint8_t i=0;i<length;++i)add(data[i]);
        }
        uint32_t value()const{return state_^0xFFFFFFFFUL;}
    private:
        uint32_t state_;
    };

    // A bounded receipt bitmap for a 32-slot block. Idempotent on duplicated
    // frames: the reported count changes only on the first valid reception.
    class Receipt {
    public:
        Receipt():bitmap_(0),accepted_(0){}
        void reset(){bitmap_=0;accepted_=0;}
        bool accept(uint8_t index) {
            if(index>=MAX_REPORTED_PACKETS)return false;
            const uint32_t mask=static_cast<uint32_t>(1UL)<<index;
            if(bitmap_&mask)return false;
            bitmap_|=mask;
            ++accepted_;
            return true;
        }
        uint32_t bitmap()const{return bitmap_;}
        uint8_t accepted()const{return accepted_;}
        uint8_t missing(uint8_t expected)const {
            return (expected>=accepted_ && expected<=MAX_REPORTED_PACKETS)?
                   static_cast<uint8_t>(expected-accepted_):0U;
        }
    private:
        uint32_t bitmap_;
        uint8_t accepted_;
    };
}
#endif
