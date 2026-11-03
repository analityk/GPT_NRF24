#ifndef GPT_NRF24_ACK_PAYLOAD_PHY_H
#define GPT_NRF24_ACK_PAYLOAD_PHY_H
#include <stdint.h>

// Event-driven 32-byte RF exchange. Reusable with a mocked Radio, Delay,
// Clock and with the real AVR adapters. No AVR headers, no heap, no UART.
// PTX transmits one packet; the full 32-byte reply arrives INSIDE hardware
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
        : radio_(radio),delay_(delay),clock_(clock) {}

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

    // Deadline includes all configured auto-retransmissions (0x23, 3 * 750us).
    // A 8ms watchdog is a failure bound, not an inter-packet pause.
    TxResult transfer(const uint8_t (&out)[32],uint8_t (&reply)[32]) {
        TxResult result;
        if (!radio_.writePayload(out)) return result;
        radio_.ceHigh();
        delay_.us(15); // CE pulse >=10us; standby -> TX takes ~130us
        radio_.ceLow();

        const uint32_t begin=clock_.now();
        uint8_t flags=0;
        for (;;) {
            if (!radio_.status(flags)) return result;
            if (flags&0x30) break;
            if (static_cast<uint32_t>(clock_.now()-begin)>=16000UL) {
                // Radio should have finished all retries by now.
                (void)radio_.flushTx();
                (void)radio_.clearIrq(0x70);
                result.code=TxCode::TIMEOUT;
                return result;
            }
        }
        uint8_t observe=0;
        if (!radio_.readRegister(Radio::OBSERVE_TX,observe)) return result;
        result.retries=static_cast<uint8_t>(observe&0x0F);
        if (flags&0x10) {
            if (!radio_.flushTx() || !radio_.clearIrq(0x70)) return result;
            result.code=TxCode::MAX_RT;
            return result;
        }

        const RxCode rx=readReply(reply);
        if (!radio_.clearIrq(0x70)) return result;
        if (rx==RxCode::GOOD) result.code=TxCode::ACK_WITH_DATA;
        else if (rx==RxCode::EMPTY) result.code=TxCode::ACK_EMPTY;
        else if (rx==RxCode::BAD_SIZE) result.code=TxCode::BAD_REPLY_SIZE;
        return result;
    }

    // PRX side: read the FIFO, never infer receipt merely from IRQ state.
    RxCode receive(uint8_t (&data)[32]) {
        uint8_t fifo=0;
        if (!radio_.readRegister(Radio::FIFO_STATUS,fifo))
            return RxCode::IO_ERROR;
        if (fifo&0x01) return RxCode::EMPTY;
        uint8_t width=0;
        if (!radio_.readDynamicPayload(data,width)) return RxCode::IO_ERROR;
        return width==32?RxCode::GOOD:RxCode::BAD_SIZE;
    }

    // Hardware ACK payload is FIFO-queued in advance. If the queue is full,
    // the caller must retry later, not overwrite an untransmitted payload.
    QueueCode queueReply(const uint8_t (&data)[32]) {
        uint8_t fifo=0;
        if (!radio_.readRegister(Radio::FIFO_STATUS,fifo))
            return QueueCode::IO_ERROR;
        if (fifo&0x20) return QueueCode::FULL;
        return radio_.writeAckPayload(0,data,32)?
               QueueCode::QUEUED:QueueCode::IO_ERROR;
    }

    bool clearPrxFlags() { return radio_.clearIrq(0x70); }

private:
    RxCode readReply(uint8_t (&data)[32]) {
        uint8_t fifo=0;
        if (!radio_.readRegister(Radio::FIFO_STATUS,fifo))
            return RxCode::IO_ERROR;
        if (fifo&0x01) return RxCode::EMPTY;
        uint8_t width=0;
        if (!radio_.readDynamicPayload(data,width)) return RxCode::IO_ERROR;
        return width==32?RxCode::GOOD:RxCode::BAD_SIZE;
    }

    Radio& radio_;
    Delay& delay_;
    Clock& clock_;
};
#endif
