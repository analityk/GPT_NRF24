#ifndef GPT_NRF24_PP_ECHO_RAMP_PORT_H
#define GPT_NRF24_PP_ECHO_RAMP_PORT_H

#include <stdint.h>
#include "byte_ring.h"

// Portable diagnostic data source/sink for the EXISTING paired stream.
// A: time-paced 00,01,...,FF source -> radio; radio echo either to binary
// UART TX (interactive ramp) or verified silently in RAM (RF saturation).
// B: radio RX -> bounded local queue -> radio ACK Payload in the next turn.
// Physical PP/session/Go-back-N mechanics remain in UartReliableStream.
// No diagnostic text is mixed with the binary UART output.
//
// The supplied monotonic clock has TicksPerUs ticks per microsecond. This
// class owns neither the hardware timer nor the RF transceiver. service()
// must run regularly (within 2^31 ticks), including during idle RF turns.
template<class Console, uint32_t TicksPerUs=2UL>
class PpEchoRampPort {
public:
    enum class Mode : uint8_t { UART_ECHO, RADIO_STRESS };
    static_assert(TicksPerUs>0 && TicksPerUs<=100UL,
                  "Tick frequency must represent whole microseconds");
    static const uint16_t START_US=10000U;
    static const uint16_t STEP_US=100U;
    static const uint8_t HIGH_WATER=120U; // 120 of 127 usable bytes

    explicit PpEchoRampPort(Console& serial)
        :serial_(serial),leader_(false),mode_(Mode::UART_ECHO),
         periodUs_(START_US),
         nextDue_(0),nextByte_(0),expectedEcho_(0),
         generated_(0),pressure_(0),echoed_(0),echoGaps_(0),
         received_(0),forwarded_(0) {}

    void configure(bool leader,uint32_t nowTicks,
                   Mode mode=Mode::UART_ECHO) {
        leader_=leader;
        mode_=mode;
        pending_.reset();
        periodUs_=(mode_==Mode::RADIO_STRESS && leader_)?0:START_US;
        nextDue_=nowTicks+periodTicks();
        nextByte_=0;
        expectedEcho_=0;
        generated_=0;
        pressure_=0;
        echoed_=0;
        echoGaps_=0;
        received_=0;
        forwarded_=0;
    }

    // On A, only 's' and 'w' from UART are commands, never stream payload.
    // A zero interval means "as fast as the bounded radio source queue can
    // drain"; an overfull source never skips a number or overwrites data.
    void service(uint32_t nowTicks,bool producerPermitted=true) {
        if(!leader_)return;
        uint8_t command=0;
        while(serial_.read(command)) {
            if(command=='s') {
                if(periodUs_>=STEP_US)
                    periodUs_=static_cast<uint16_t>(periodUs_-STEP_US);
                nextDue_=nowTicks+periodTicks();
            } else if(command=='w') {
                if(periodUs_<=START_US-STEP_US)
                    periodUs_=static_cast<uint16_t>(periodUs_+STEP_US);
                nextDue_=nowTicks+periodTicks();
            }
        }
        // Pause at the *producer* boundary. Existing queued bytes, sequence
        // number and timer phase are preserved across a temporary RF outage.
        // Avoid a catch-up burst when the link eventually recovers.
        if(!producerPermitted) {
            nextDue_=nowTicks+periodTicks();
            return;
        }
        if(periodUs_==0) {
            while(pending_.available()<HIGH_WATER)produce();
            ++pressure_; // offered rate is beyond the bounded queue limit
            return;
        }
        // Catch up after a long radio turn, but never exceed the high-water
        // mark. Reset the due time on pressure to avoid a deferred burst.
        while(reached(nowTicks,nextDue_)) {
            if(pending_.available()>=HIGH_WATER) {
                ++pressure_;
                nextDue_=nowTicks+periodTicks();
                break;
            }
            produce();
            nextDue_+=periodTicks();
        }
    }

    // Port interface required by UartReliableStream<Port>.
    uint8_t available()const{return pending_.available();}
    uint8_t inputFreeSpace()const{return pending_.freeSpace();}
    bool read(uint8_t& b) {
        if(!pending_.pop(b))return false;
        if(!leader_)++forwarded_;
        return true;
    }
    uint8_t freeSpace()const {
        // In stress mode the radio echo is accepted in RAM, not into the
        // slow console TX ring. Its credit must not depend on UART speed.
        return leader_?
            (mode_==Mode::RADIO_STRESS?127U:serial_.freeSpace()):
            pending_.freeSpace();
    }
    bool writeAll(const uint8_t* bytes,uint8_t count) {
        if(count && !bytes)return false;
        if(leader_) {
            // UART_ECHO requires atomic UART TX acceptance; RADIO_STRESS
            // consumes the bytes in RAM without UART backpressure. On a
            // mismatch, resynchronize to count one sequence discontinuity.
            if(mode_==Mode::UART_ECHO && !serial_.writeAll(bytes,count))
                return false;
            for(uint8_t i=0;i<count;++i) {
                if(bytes[i]!=expectedEcho_)++echoGaps_;
                expectedEcho_=static_cast<uint8_t>(bytes[i]+1U);
                ++echoed_;
            }
            return true;
        }
        if(pending_.freeSpace()<count)return false;
        for(uint8_t i=0;i<count;++i)
            (void)pending_.push(bytes[i]); // checked atomically as a group
        received_+=count;
        return true;
    }

    // Debugger-visible observations; not sent to the binary UART.
    uint16_t periodUs()const{return periodUs_;}
    uint32_t generated()const{return generated_;}
    uint32_t pressureEvents()const{return pressure_;}
    uint32_t echoed()const{return echoed_;}
    uint32_t echoGaps()const{return echoGaps_;}
    uint32_t received()const{return received_;}
    uint32_t forwarded()const{return forwarded_;}

private:
    static bool reached(uint32_t now,uint32_t deadline) {
        return static_cast<uint32_t>(now-deadline)<0x80000000UL;
    }
    uint32_t periodTicks()const {
        return static_cast<uint32_t>(periodUs_)*TicksPerUs;
    }
    void produce() {
        // All calls have checked the bounded queue's high-water mark.
        if(pending_.push(nextByte_)) {
            ++nextByte_; // uint8_t wraps from 255 back to zero
            ++generated_;
        }
    }
    Console& serial_;
    ByteRing<128> pending_;
    bool leader_;
    Mode mode_;
    uint16_t periodUs_;
    uint32_t nextDue_;
    uint8_t nextByte_,expectedEcho_;
    uint32_t generated_,pressure_,echoed_,echoGaps_;
    uint32_t received_,forwarded_;
};

#endif
