#ifndef GPT_NRF24_PP_RECOVERY_CONTROL_H
#define GPT_NRF24_PP_RECOVERY_CONTROL_H
#include <stdint.h>

// Link liveness belongs to the transport, not to PTX/PRX hardware or UART.
// Silence does NOT prove that the peer rebooted: retain SID, sequence
// numbers and unacknowledged frames until an actual identity change.
// Call poll() frequently enough that 32-bit elapsed ticks cannot wrap twice.
class PpRecoveryControl {
public:
    enum class State : uint8_t { CONNECTING=0, HEALTHY=1,
                                  SUSPECT=2, OFFLINE=3 };
    enum class UncertainPolicy : uint8_t { HOLD=0, RETRY_AT_LEAST_ONCE=1 };

    PpRecoveryControl(uint32_t expectedTurnTicks=3200UL,
                      uint32_t offlineTicks=2000000UL,
                      UncertainPolicy policy=UncertainPolicy::HOLD,
                      uint32_t warmGraceTicks=10000000UL)
       :expected_(expectedTurnTicks),offline_(offlineTicks),
        warmGrace_(warmGraceTicks),lastSeen_(0),
        policy_(policy),state_(State::CONNECTING),uncertain_(false),
        held_(false),warm_(0),cold_(0),suspicions_(0),expired_(0) {}

    void reset(uint32_t now) {
        lastSeen_=now;
        state_=State::CONNECTING;
        uncertain_=false;
        held_=false;
        warm_=cold_=suspicions_=expired_=0;
    }
    void established(uint32_t now) {
        lastSeen_=now;
        state_=State::HEALTHY;
    }
    // Only a validated frame belonging to the negotiated SID proves that
    // the old peer state still exists; an empty hardware ACK does not.
    void validPeerFrame(uint32_t now) {
        if(state_==State::SUSPECT || state_==State::OFFLINE)++warm_;
        lastSeen_=now;
        state_=State::HEALTHY;
    }
    void poll(uint32_t now) {
        if(state_!=State::HEALTHY && state_!=State::SUSPECT)return;
        const uint32_t elapsed=static_cast<uint32_t>(now-lastSeen_);
        if(elapsed>=offline_)state_=State::OFFLINE;
        else if(elapsed>=expected_*3UL && state_==State::HEALTHY) {
            state_=State::SUSPECT;
            ++suspicions_;
        }
    }
    // Called only for a proven new peer identity / abandoned wire session.
    // ACK ambiguity must survive a successful new handshake until the host
    // explicitly acknowledges it.  It cannot be solved by nRF auto-ACK.
    void peerStateLost(uint32_t now,bool hadUnacknowledged) {
        lastSeen_=now;
        state_=State::CONNECTING;
        ++cold_;
        if(hadUnacknowledged) {
            uncertain_=true;
            held_=(policy_==UncertainPolicy::HOLD);
        }
    }
    // Silence cannot identify the peer. After a bounded grace period we
    // explicitly ABANDON warm state and request a NEW handshake. That is a
    // local policy decision, not evidence that the peer actually rebooted.
    bool warmExpired(uint32_t now)const {
        return state_==State::OFFLINE &&
               static_cast<uint32_t>(now-lastSeen_)>=warmGrace_;
    }
    void abandonWarm(uint32_t now,bool hadUnacknowledged) {
        peerStateLost(now,hadUnacknowledged);
        ++expired_;
    }
    bool canProduce()const {return state_==State::HEALTHY && !held_;}
    bool canSendData()const {return state_==State::HEALTHY && !held_;}
    bool uncertainty()const{return uncertain_;}
    bool held()const{return held_;}
    // Host chooses to retry cached unacknowledged DATA at least once. This
    // may duplicate bytes already committed by a peer before its reset.
    void retryUncertain() {held_=false;}
    void acknowledgeUncertainty() {uncertain_=false;}
    State state()const{return state_;}
    uint16_t warmRecoveries()const{return warm_;}
    uint16_t coldRecoveries()const{return cold_;}
    uint16_t suspicions()const{return suspicions_;}
    uint16_t expiredRecoveries()const{return expired_;}
private:
    uint32_t expected_,offline_,warmGrace_,lastSeen_;
    UncertainPolicy policy_;
    State state_;
    bool uncertain_,held_;
    uint16_t warm_,cold_,suspicions_,expired_;
};
#endif
