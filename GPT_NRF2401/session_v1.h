#ifndef GPT_NRF24_SESSION_V1_H
#define GPT_NRF24_SESSION_V1_H

#include <stdint.h>
#include "frame_v1.h"

// Two-node discovery/handshake for a single hop. Neither timer nor radio
// policy lives here: caller repeats requests on a bounded TX/RX schedule.
// Local and peer must have distinct logical classes. A smaller class initiates.
// Nonces are boot-unique epochs supplied externally by the board platform.
// SID is CRC8: useful corruption/stale-frame discriminator, NOT cryptographic
// authentication or a collision-free lifetime identifier.
class SessionV1 {
public:
    enum class State : uint8_t {
        DISCOVER, WAIT_READY, WAIT_READY_ACK, ACTIVE
    };
    SessionV1(uint8_t local, uint8_t peer, uint32_t bootEpoch)
        : local_(local),peer_(peer),leader_(local<peer),
          ownNonce_(bootEpoch),peerNonce_(0),sid_(0),
          state_(State::DISCOVER),response_(0),generation_(0) {}

    bool active() const { return state_==State::ACTIVE; }
    uint8_t sid() const { return sid_; }
    uint16_t generation() const { return generation_; }
    bool isLeader() const { return leader_; }

    // Leader restarts discovery following repeated unanswered TX slots.
    // This changes the candidate nonce even without an MCU reboot.
    void restart() {
        ++ownNonce_;
        if (ownNonce_==0) ++ownNonce_;
        peerNonce_=0;
        sid_=0;
        response_=0;
        state_=State::DISCOVER;
    }

    // Caller sends this frame on its next PHY slot; false means listen.
    bool prepare(uint8_t* raw) const {
        if (!raw) return false;
        uint8_t op=0;
        if (leader_) {
            if (state_==State::DISCOVER) op=1;        // HELLO
            if (state_==State::WAIT_READY_ACK) op=3; // READY
        } else {
            op=response_; // HELLO_REPLY or READY_ACK
        }
        if (!op) return false;
        FrameV1::Header h;
        h.kind=FrameV1::Kind::CONTROL;
        h.src=local_;h.dst=peer_;h.sid=op>=3 ? sid_:0;
        h.txSeq=0;h.rxLast=0xFF;
        uint8_t body[24]={};
        body[0]=op;
        if (op==1) {
            put32(body+1,ownNonce_);
            body[5]=0x03; body[6]=0; // RELIABLE + DATA_ACK
            h.length=7;
        } else if (op==2) {
            put32(body+1,ownNonce_);
            put32(body+5,peerNonce_);
            body[9]=0x03;body[10]=0;
            h.length=11;
        } else if (op==3) {
            put32(body+1,ownNonce_);
            put32(body+5,peerNonce_);
            body[9]=sid_;
            h.length=10;
        } else if (op==4) {
            body[1]=sid_;
            h.length=2;
        }
        return FrameV1::encode(h,body,raw)==FrameV1::Error::OK;
    }

    // B sends replies once per received request. If an ACK was lost,
    // the repeated HELLO or READY makes us send the reply again.
    void sentReply() {
        if (!leader_) response_=0;
    }

    // Validated CONTROL view only. Returns true iff the frame was relevant.
    bool accept(const FrameV1::View& frame) {
        const FrameV1::Header& h=frame.header;
        if (h.kind!=FrameV1::Kind::CONTROL ||
            h.src!=peer_ || h.dst!=local_) return false;
        const uint8_t op=frame.body[0];
        if (leader_) {
            if (op==2 && h.sid==0 && h.length==11 &&
                state_==State::DISCOVER &&
                get32(frame.body+5)==ownNonce_ &&
                (frame.body[9]&0x03)==0x03) {
                peerNonce_=get32(frame.body+1);
                sid_=derive(ownNonce_,peerNonce_);
                state_=State::WAIT_READY_ACK;
                return true;
            }
            if (op==4 && h.length==2 &&
                state_==State::WAIT_READY_ACK &&
                h.sid==sid_ && frame.body[1]==sid_) {
                state_=State::ACTIVE;
                ++generation_;
                return true;
            }
        } else {
            if (op==1 && h.sid==0 && h.length==7 &&
                (frame.body[5]&0x03)==0x03) {
                const uint32_t nonce=get32(frame.body+1);
                if (state_==State::ACTIVE && nonce==peerNonce_) return true;
                peerNonce_=nonce;
                sid_=derive(peerNonce_,ownNonce_);
                state_=State::WAIT_READY;
                response_=2;
                return true;
            }
            if (op==3 && h.length==10 &&
                (state_==State::WAIT_READY || state_==State::ACTIVE) &&
                get32(frame.body+1)==peerNonce_ &&
                get32(frame.body+5)==ownNonce_ &&
                frame.body[9]==sid_ && h.sid==sid_) {
                if (state_!=State::ACTIVE) {
                    state_=State::ACTIVE;
                    ++generation_;
                }
                response_=4;
                return true;
            }
        }
        return false;
    }

    static uint8_t derive(uint32_t nonceA,uint32_t nonceB,
                          uint8_t a=0,uint8_t b=1) {
        uint8_t bytes[10];
        bytes[0]=a;bytes[1]=b;
        put32(bytes+2,nonceA);
        put32(bytes+6,nonceB);
        uint8_t crc=0;
        for (uint8_t i=0;i<10;++i) {
            crc=static_cast<uint8_t>(crc^bytes[i]);
            for (uint8_t bit=0;bit<8;++bit) {
                crc=static_cast<uint8_t>((crc&0x80U)
                    ? (crc<<1)^0x07U : crc<<1);
            }
        }
        return crc==0?1:crc;
    }

private:
    static void put32(uint8_t* p,uint32_t v) {
        for(uint8_t i=0;i<4;++i) p[i]=static_cast<uint8_t>(v>>(i*8U));
    }
    static uint32_t get32(const uint8_t* p) {
        uint32_t v=0;
        for(uint8_t i=0;i<4;++i) v |= (static_cast<uint32_t>(p[i])<<(i*8U));
        return v;
    }

    uint8_t local_,peer_;
    bool leader_;
    uint32_t ownNonce_,peerNonce_;
    uint8_t sid_;
    State state_;
    uint8_t response_;
    uint16_t generation_;
};
#endif
