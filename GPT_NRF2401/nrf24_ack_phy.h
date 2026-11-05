#ifndef GPT_NRF24_ACK_PAYLOAD_PHY_H
#define GPT_NRF24_ACK_PAYLOAD_PHY_H
#include <stdint.h>

// One-step-at-a-time DPL RF exchange. Reusable with a mocked Radio, Delay,
// Clock and with the real AVR adapters. No AVR headers, no heap, no UART.
// PTX transmits a packet; the reply arrives INSIDE hardware
// Enhanced ShockBurst ACK. PRX never changes its radio mode per packet.
// Timer Clock must return monotonic modulo-2^32 ticks at 2 MHz.
#define NRF24_PHY_MIN_FRAME_BYTES 8U
#define NRF24_PHY_MAX_FRAME_BYTES 32U
#define NRF24_PHY_POWERUP_MS 2U
#define NRF24_PHY_PRX_SETTLE_US 150U
#define NRF24_PHY_PRX_REKEY_GUARD_US 550U
#define NRF24_PHY_PTX_GUARD_TICKS 260UL
#define NRF24_PHY_CE_PULSE_TICKS 30UL
#define NRF24_PHY_TX_TIMEOUT_TICKS 16000UL
#define NRF24_PHY_CONFIG_PTX 0x0EU
#define NRF24_PHY_CONFIG_PRX 0x0FU
#define NRF24_PHY_IRQ_CLEAR_MASK 0x70U
#define NRF24_PHY_IRQ_TX_DONE_MASK 0x20U
#define NRF24_PHY_IRQ_MAX_RT_MASK 0x10U
#define NRF24_PHY_OBSERVE_RETRY_MASK 0x0FU
#define NRF24_PHY_RX_EMPTY_MASK 0x01U
#define NRF24_PHY_RX_FULL_MASK 0x02U
#define NRF24_PHY_TX_FULL_MASK 0x20U
#define NRF24_PHY_ACK_PIPE 0U
#define NRF24_PHY_TIMING_MAX_TICKS 0xFFFFUL

template<class Radio,class Delay,class Clock,bool MeasureTiming=false>
class NrfAckPayloadPhy {
public:
    enum class TxCode : uint8_t {
        ACK_WITH_DATA, ACK_EMPTY, BAD_REPLY_SIZE, MAX_RT, TIMEOUT, IO_ERROR
    };
    enum class RxCode : uint8_t { EMPTY, GOOD, BAD_SIZE, IO_ERROR };
    enum class QueueCode : uint8_t { QUEUED, FULL, IO_ERROR };
    enum class TxStep : uint8_t { IDLE, PENDING, COMPLETE };
    enum class TxPhase : uint8_t { IDLE, GUARD, CE_PULSE, ON_AIR };
    struct TxResult {
        TxCode code;
        uint8_t retries;
        TxResult():code(TxCode::IO_ERROR),retries(0) {}
    };
    struct Timing {
        uint16_t loadTicks,guardTicks,ceTicks,waitTicks,readTicks;
        // ON_AIR status polling is part of waitTicks; these counts and
        // durations must never be added to the total exchange period.
        uint16_t pollCount,pollSpiTicks;
        // Disjoint portions of readTicks, from the terminal IRQ onward.
        uint16_t observeTicks,fifoTicks,payloadTicks,irqTicks;
        Timing():loadTicks(0),guardTicks(0),ceTicks(0),
                 waitTicks(0),readTicks(0),pollCount(0),pollSpiTicks(0),
                 observeTicks(0),fifoTicks(0),payloadTicks(0),irqTicks(0){}
    };

    NrfAckPayloadPhy(Radio& radio,Delay& delay,Clock& clock)
        : radio_(radio),delay_(delay),clock_(clock),lastCompleted_(0),
          phaseStarted_(0),haveLast_(false),rxFifoFullSamples_(0),
          phase_(TxPhase::IDLE) {}

    // Number of receive polls where the existing FIFO_STATUS read saw all
    // three hardware RX slots occupied. This is NOT a count of dropped
    // packets: the chip has no direct software-visible RX-overflow counter.
    uint16_t rxFifoFullSamples()const{return rxFifoFullSamples_;}
    TxPhase txPhase()const{return phase_;}
    bool txBusy()const{return phase_!=TxPhase::IDLE;}
    bool txOnAir()const{return phase_==TxPhase::ON_AIR;}
    const Timing& lastTiming()const{return timing_;}
    uint32_t lastCompletedAt()const{return lastCompleted_;}

    // Power up exactly once. Subsequent TX operations remain in PTX and
    // incur no 1.5/2 ms oscillator start penalties.
    bool beginPtx() {
        radio_.ceLow();
        if (!radio_.writeRegister(Radio::CONFIG,NRF24_PHY_CONFIG_PTX)) return false;
        delay_.ms(NRF24_PHY_POWERUP_MS);
        return radio_.clearIrq(NRF24_PHY_IRQ_CLEAR_MASK);
    }
    bool beginPrx() {
        radio_.ceLow();
        if (!radio_.writeRegister(Radio::CONFIG,NRF24_PHY_CONFIG_PRX)) return false;
        delay_.ms(NRF24_PHY_POWERUP_MS);
        if (!radio_.clearIrq(NRF24_PHY_IRQ_CLEAR_MASK)) return false;
        radio_.ceHigh();
        delay_.us(NRF24_PHY_PRX_SETTLE_US);
        return true;
    }

    // Submit one complete frame to the radio's FIFO. No copy of the 32B
    // payload is kept in PHY; the caller may reuse its buffer immediately.
    // GUARD and CE_PULSE are driven by pollTransfer(), never by busy waits.
    bool startTransfer(const uint8_t (&out)[NRF24_PHY_MAX_FRAME_BYTES],
                       uint8_t txWidth) {
        if(txBusy() || txWidth<NRF24_PHY_MIN_FRAME_BYTES ||
           txWidth>NRF24_PHY_MAX_FRAME_BYTES)return false;
        const uint32_t started=MeasureTiming?clock_.now():0;
        if(!radio_.writeDynamicPayload(out,txWidth))return false;
        if(MeasureTiming) {
            const uint32_t loaded=clock_.now();
            timing_=Timing();
            timing_.loadTicks=measuredSpan(loaded-started);
            phaseStarted_=loaded;
        }
        phase_=TxPhase::GUARD;
        return true;
    }

    // At most one status SPI transaction per call; reads the reply as soon
    // as TX_DS/MAX_RT appears. The radio runs during the caller's other
    // work. replyWidth comes from hardware DPL, not a payload LEN field.
    // This method MUST be serviced frequently: CE needs >=15us high, and
    // the receiver's ACK is pulled out before a new frame can start.
    TxStep pollTransfer(uint8_t (&reply)[NRF24_PHY_MAX_FRAME_BYTES],
                        uint8_t& replyWidth,TxResult& result) {
        if(phase_==TxPhase::IDLE)return TxStep::IDLE;
        if(phase_==TxPhase::GUARD) {
            replyWidth=0;
            result=TxResult();
            if(haveLast_ &&
               static_cast<uint32_t>(clock_.now()-lastCompleted_)<
                   NRF24_PHY_PTX_GUARD_TICKS)return TxStep::PENDING;
            if(MeasureTiming)
                timing_.guardTicks=measuredSpan(clock_.now()-phaseStarted_);
            radio_.ceHigh();
            phaseStarted_=clock_.now();
            phase_=TxPhase::CE_PULSE;
            return TxStep::PENDING;
        }
        if(phase_==TxPhase::CE_PULSE) {
            if(static_cast<uint32_t>(clock_.now()-phaseStarted_)<
                   NRF24_PHY_CE_PULSE_TICKS)return TxStep::PENDING;
            if(MeasureTiming)
                timing_.ceTicks=measuredSpan(clock_.now()-phaseStarted_);
            radio_.ceLow();
            phaseStarted_=clock_.now();
            phase_=TxPhase::ON_AIR;
            return TxStep::PENDING;
        }
        replyWidth=0;
        uint8_t flags=0;
        const uint32_t statusStart=MeasureTiming?clock_.now():0;
        if(!radio_.status(flags))return finishError(result);
        if(MeasureTiming) {
            ++timing_.pollCount;
            const uint32_t elapsed=clock_.now()-statusStart;
            const uint32_t cumulative=static_cast<uint32_t>(
                timing_.pollSpiTicks)+elapsed;
            timing_.pollSpiTicks=measuredSpan(cumulative);
        }
        if((flags&(NRF24_PHY_IRQ_TX_DONE_MASK|
                   NRF24_PHY_IRQ_MAX_RT_MASK))==0) {
            const uint32_t now=clock_.now();
            if(static_cast<uint32_t>(now-phaseStarted_)<
                   NRF24_PHY_TX_TIMEOUT_TICKS)return TxStep::PENDING;
            if(MeasureTiming)markTerminal(now);
            result.code=TxCode::TIMEOUT;
            if(!radio_.flushTx() ||
               !radio_.clearIrq(NRF24_PHY_IRQ_CLEAR_MASK))
                result.code=TxCode::IO_ERROR;
            return finish();
        }
        if(MeasureTiming)markTerminal(clock_.now());
        uint32_t stageStarted=phaseStarted_;
        uint8_t observe=0;
        if(!radio_.readRegister(Radio::OBSERVE_TX,observe))
            return finishError(result);
        if(MeasureTiming)markReadStage(stageStarted,timing_.observeTicks);
        result.retries=static_cast<uint8_t>(observe&NRF24_PHY_OBSERVE_RETRY_MASK);
        if(flags&NRF24_PHY_IRQ_MAX_RT_MASK) {
            if(!radio_.flushTx() ||
               !radio_.clearIrq(NRF24_PHY_IRQ_CLEAR_MASK))
                return finishError(result);
            result.code=TxCode::MAX_RT;
            return finish();
        }
        uint8_t fifo=0;
        if(!radio_.readRegister(Radio::FIFO_STATUS,fifo))
            return finishError(result);
        if(MeasureTiming)markReadStage(stageStarted,timing_.fifoTicks);
        if(fifo&NRF24_PHY_RX_EMPTY_MASK) {
            result.code=TxCode::ACK_EMPTY;
        } else {
            const bool good=radio_.readDynamicPayload(reply,replyWidth);
            if(MeasureTiming)markReadStage(stageStarted,timing_.payloadTicks);
            if(!good) {
                result.code=(replyWidth==0 ||
                             replyWidth>NRF24_PHY_MAX_FRAME_BYTES)?
                          TxCode::BAD_REPLY_SIZE:TxCode::IO_ERROR;
            } else {
                result.code=(replyWidth>=NRF24_PHY_MIN_FRAME_BYTES)?
                            TxCode::ACK_WITH_DATA:TxCode::BAD_REPLY_SIZE;
            }
        }
        const bool irqCleared=radio_.clearIrq(NRF24_PHY_IRQ_CLEAR_MASK);
        if(MeasureTiming)markReadStage(stageStarted,timing_.irqTicks);
        if(!irqCleared)
            result.code=TxCode::IO_ERROR;
        return finish();
    }

    RxCode receive(uint8_t (&data)[NRF24_PHY_MAX_FRAME_BYTES],uint8_t& width) {
        width=0;
        uint8_t fifo=0;
        if (!radio_.readRegister(Radio::FIFO_STATUS,fifo))
            return RxCode::IO_ERROR;
        if(fifo&NRF24_PHY_RX_FULL_MASK)++rxFifoFullSamples_;
        if (fifo&NRF24_PHY_RX_EMPTY_MASK) return RxCode::EMPTY;
        if (!radio_.readDynamicPayload(data,width))
            return (width==0 || width>NRF24_PHY_MAX_FRAME_BYTES)?
                   RxCode::BAD_SIZE:RxCode::IO_ERROR;
        return width>=NRF24_PHY_MIN_FRAME_BYTES?RxCode::GOOD:RxCode::BAD_SIZE;
    }

    QueueCode queueReply(const uint8_t (&data)[NRF24_PHY_MAX_FRAME_BYTES],
                         uint8_t width) {
        if (width<NRF24_PHY_MIN_FRAME_BYTES ||
            width>NRF24_PHY_MAX_FRAME_BYTES)return QueueCode::IO_ERROR;
        uint8_t fifo=0;
        if (!radio_.readRegister(Radio::FIFO_STATUS,fifo))
            return QueueCode::IO_ERROR;
        if (fifo&NRF24_PHY_TX_FULL_MASK) return QueueCode::FULL;
        return radio_.writeAckPayload(NRF24_PHY_ACK_PIPE,data,width)?
               QueueCode::QUEUED:QueueCode::IO_ERROR;
    }

    // Discard stale FIFO content on an epoch transition. The PTX keeps its
    // role and oscillator running; there is no repeated power-up penalty.
    bool resetPtxFifos() {
        radio_.ceLow();
        phase_=TxPhase::IDLE;
        lastCompleted_=clock_.now();haveLast_=true;
        return radio_.flushTx() && radio_.flushRx() &&
               radio_.clearIrq(NRF24_PHY_IRQ_CLEAR_MASK);
    }

    // ACK Payload is prepared BEFORE the matching incoming packet. When
    // changing the session or response opcode, a queued old ACK must NOT
    // survive into the next epoch. Complete the possible 32B on-air ACK
    // (130us turnaround + ~329us frame) before flushing in standby.
    // The brief PRX blackout is permitted at a handshake boundary only.
    bool replacePrxReply(const uint8_t (&data)[NRF24_PHY_MAX_FRAME_BYTES],
                         uint8_t width) {
        if(width<NRF24_PHY_MIN_FRAME_BYTES ||
           width>NRF24_PHY_MAX_FRAME_BYTES)return false;
        radio_.ceLow();
        delay_.us(NRF24_PHY_PRX_REKEY_GUARD_US);
        const bool ok=radio_.flushTx() && radio_.flushRx() &&
                      radio_.clearIrq(NRF24_PHY_IRQ_CLEAR_MASK) &&
                      radio_.writeAckPayload(NRF24_PHY_ACK_PIPE,data,width);
        radio_.ceHigh();
        delay_.us(NRF24_PHY_PRX_SETTLE_US);
        return ok;
    }

    bool clearPrxFlags() { return radio_.clearIrq(NRF24_PHY_IRQ_CLEAR_MASK); }

private:
    static uint16_t measuredSpan(uint32_t ticks) {
        return static_cast<uint16_t>(ticks>NRF24_PHY_TIMING_MAX_TICKS?
                                     NRF24_PHY_TIMING_MAX_TICKS:ticks);
    }
    void markTerminal(uint32_t now) {
        timing_.waitTicks=measuredSpan(now-phaseStarted_);
        phaseStarted_=now;
    }
    void markReadStage(uint32_t& started,uint16_t& elapsedTicks) {
        const uint32_t now=clock_.now();
        elapsedTicks=measuredSpan(now-started);
        started=now;
    }
    TxStep finish() {
        const uint32_t now=clock_.now();
        if(MeasureTiming)
            timing_.readTicks=measuredSpan(now-phaseStarted_);
        lastCompleted_=now;
        haveLast_=true;
        phase_=TxPhase::IDLE;
        return TxStep::COMPLETE;
    }
    TxStep finishError(TxResult& result) {
        result.code=TxCode::IO_ERROR;
        result.retries=0;
        (void)radio_.flushTx();
        (void)radio_.clearIrq(NRF24_PHY_IRQ_CLEAR_MASK);
        return finish();
    }
    Radio& radio_;
    Delay& delay_;
    Clock& clock_;
    uint32_t lastCompleted_;
    uint32_t phaseStarted_;
    bool haveLast_;
    uint16_t rxFifoFullSamples_;
    TxPhase phase_;
    Timing timing_;
};
#endif
