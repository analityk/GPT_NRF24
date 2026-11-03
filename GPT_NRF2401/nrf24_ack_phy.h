#ifndef GPT_NRF24_ACK_PAYLOAD_PHY_H
#define GPT_NRF24_ACK_PAYLOAD_PHY_H
#include <stdint.h>

// Event-driven DPL RF exchange with an 8-byte minimum payload. Reusable with a mocked Radio, Delay,
// Clock and with the real AVR adapters. No AVR headers, no heap, no UART.
// PTX transmits a packet; the reply arrives INSIDE hardware
// Enhanced ShockBurst ACK. PRX never changes its radio mode per packet.
// Timer Clock must return monotonic modulo-2^32 ticks at 2 MHz.
template<class Radio,class Delay,class Clock>
class NrfAckPayloadPhy {
public:
    enum class TxCode : uint8_t {
        ACK_WITH_DATA, ACK_EMPTY, BAD_REPLY_SIZE, MAX_RT, TIMEOUT, IO_ERROR
    };
    enum class RxCode : uint8_t { EMPTY, GOOD, BAD_SIZE, IO_ERROR };
    enum class QueueCode : uint8_t { QUEUED, FULL, IO_ERROR };
    struct TxResult {
        TxCode code;
        uint8_t retries;
        TxResult():code(TxCode::IO_ERROR),retries(0) {}
    };

    NrfAckPayloadPhy(Radio& radio,Delay& delay,Clock& clock)
        : radio_(radio),delay_(delay),clock_(clock),lastCompleted_(0),haveLast_(false) {}

    // Power up exactly once. Subsequent TX operations remain in PTX and
    // incur no 1.5/2 ms oscillator start penalties.
    bool beginPtx() {
        radio_.ceLow();
        if (!radio_.writeRegister(Radio::CONFIG,0x0E)) return false;
        delay_.ms(2);
        return radio_.clearIrq(0x70);
    }
    bool beginPrx() {
        radio_.ceLow();
        if (!radio_.writeRegister(Radio::CONFIG,0x0F)) return false;
        delay_.ms(2);
        if (!radio_.clearIrq(0x70)) return false;
        radio_.ceHigh();
        delay_.us(150);
        return true;
    }

    // Frames have an 8-byte diagnostic header plus 0..24 opaque bytes.
    // replyWidth comes from the hardware DPL register, never from an
    // application-defined LEN field.
    TxResult transfer(const uint8_t (&out)[32],uint8_t txWidth,
                      uint8_t (&reply)[32],uint8_t& replyWidth) {
        TxResult result;
        replyWidth=0;
        if (txWidth<8 || txWidth>32 ||
            !radio_.writeDynamicPayload(out,txWidth)) return result;
        if (haveLast_)
            while (static_cast<uint32_t>(clock_.now()-lastCompleted_)<260UL) {}
        radio_.ceHigh();
        delay_.us(15);
        radio_.ceLow();
        const uint32_t start=clock_.now();
        uint8_t flags=0;
        for (;;) {
            if (!radio_.status(flags)) return result;
            if (flags&0x30) break;
            if (static_cast<uint32_t>(clock_.now()-start)>=16000UL) {
                (void)radio_.flushTx();
                (void)radio_.clearIrq(0x70);
                lastCompleted_=clock_.now();haveLast_=true;
                result.code=TxCode::TIMEOUT;
                return result;
            }
        }
        uint8_t observe=0;
        if (!radio_.readRegister(Radio::OBSERVE_TX,observe)) return result;
        result.retries=static_cast<uint8_t>(observe&0x0F);
        if (flags&0x10) {
            if (!radio_.flushTx() || !radio_.clearIrq(0x70)) return result;
            lastCompleted_=clock_.now();haveLast_=true;
            result.code=TxCode::MAX_RT;
            return result;
        }
        uint8_t fifo=0;
        if (!radio_.readRegister(Radio::FIFO_STATUS,fifo)) return result;
        if (fifo&0x01) {
            result.code=TxCode::ACK_EMPTY;
        } else {
            const bool good=radio_.readDynamicPayload(reply,replyWidth);
            if (!good) {
                if (replyWidth==0 || replyWidth>32)
                    result.code=TxCode::BAD_REPLY_SIZE;
            } else {
                result.code=(replyWidth>=8)?TxCode::ACK_WITH_DATA
                                         :TxCode::BAD_REPLY_SIZE;
            }
        }
        if (!radio_.clearIrq(0x70)) {
            result.code=TxCode::IO_ERROR;
            return result;
        }
        lastCompleted_=clock_.now();haveLast_=true;
        return result;
    }

    RxCode receive(uint8_t (&data)[32],uint8_t& width) {
        width=0;
        uint8_t fifo=0;
        if (!radio_.readRegister(Radio::FIFO_STATUS,fifo))
            return RxCode::IO_ERROR;
        if (fifo&0x01) return RxCode::EMPTY;
        if (!radio_.readDynamicPayload(data,width))
            return (width==0 || width>32)?RxCode::BAD_SIZE:RxCode::IO_ERROR;
        return width>=8?RxCode::GOOD:RxCode::BAD_SIZE;
    }

    QueueCode queueReply(const uint8_t (&data)[32],uint8_t width) {
        if (width<8 || width>32) return QueueCode::IO_ERROR;
        uint8_t fifo=0;
        if (!radio_.readRegister(Radio::FIFO_STATUS,fifo))
            return QueueCode::IO_ERROR;
        if (fifo&0x20) return QueueCode::FULL;
        return radio_.writeAckPayload(0,data,width)?
               QueueCode::QUEUED:QueueCode::IO_ERROR;
    }

    bool clearPrxFlags() { return radio_.clearIrq(0x70); }

private:
    Radio& radio_;
    Delay& delay_;
    Clock& clock_;
    uint32_t lastCompleted_;
    bool haveLast_;
};
#endif
