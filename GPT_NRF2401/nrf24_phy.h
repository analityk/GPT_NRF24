#ifndef GPT_NRF24_PHY_H
#define GPT_NRF24_PHY_H

#include <stdint.h>

// Single-hop nRF24L01+ physical packet operations.
// Radio supplies register/FIFO/CE methods; Delay supplies us(uint16_t) and
// ms(uint8_t). Neither timing nor GPIO is hard-coded to a specific board.
// A caller must serialize these operations (one radio, one owner).
template <typename Radio, typename Delay>
class Nrf24Phy {
public:
    enum class TxStatus : uint8_t {
        ACKED=0, MAX_RETRIES=1, TIMEOUT=2, IO_ERROR=3
    };
    struct TxResult {
        TxStatus result;
        uint8_t status;
        uint8_t arc;
        TxResult() : result(TxStatus::IO_ERROR), status(0), arc(0) {}
    };

    Nrf24Phy(Radio& radio, Delay& delay) : radio_(radio), delay_(delay) {}

    // Power up and enter continuous PRX. CE is never raised until
    // after the 2ms power-up/settle guard.
    bool listen() {
        radio_.ceLow();
        if (!radio_.writeRegister(Radio::CONFIG, 0x0F)) return false;
        delay_.ms(2);      // >=1.5ms crystal start, also covers RX/TX switch
        radio_.ceHigh();
        delay_.us(150);    // >=130us PLL settling
        return true;
    }

    void standby() { radio_.ceLow(); }

    // One 32-byte frame. Enhanced ShockBurst automatically handles
    // ACK and up to SETUP_RETR. NO second application retry here.
    // On MAX_RT the payload remains in the FIFO: we explicitly flush it.
    TxResult transmit(const uint8_t* data) {
        TxResult t;
        if (!data) return t;

        radio_.ceLow();
        if (!radio_.writeRegister(Radio::CONFIG, 0x0E) ||
            !radio_.flushTx() || !radio_.clearIrq(0x30)) return t;
        delay_.ms(2); // covers first PWR_UP and all mode switches
        if (!radio_.writePayload(data)) return t;

        radio_.ceHigh();
        delay_.us(15); // minimum 10us
        radio_.ceLow();

        // SETUP_RETR=0x3F: 15 retries with ARD=1000us.
        // 30ms timeout covers this exchange with margin.
        for (uint8_t elapsed=0; elapsed<30; ++elapsed) {
            if (!radio_.status(t.status)) return t;
            if (t.status & 0x30) {
                uint8_t observe=0;
                if (!radio_.readRegister(Radio::OBSERVE_TX, observe)) return t;
                t.arc=static_cast<uint8_t>(observe & 0x0FU);
                if (t.status & 0x20) {
                    if (!radio_.clearIrq(0x30)) return t;
                    t.result=TxStatus::ACKED;
                    return t;
                }
                // MAX_RT doesn't remove the payload from the FIFO.
                if (!radio_.flushTx() || !radio_.clearIrq(0x30)) return t;
                t.result=TxStatus::MAX_RETRIES;
                return t;
            }
            delay_.ms(1);
        }

        if (!radio_.flushTx() || !radio_.clearIrq(0x30)) return t;
        t.result=TxStatus::TIMEOUT;
        return t;
    }

    // Poll RX FIFO, not just RX_DR: clearing one IRQ must not conceal
    // another already buffered packet. No dynamic payloads in this test.
    bool receive(uint8_t* data, bool& received) {
        received=false;
        if (!data) return false;
        uint8_t fifo=0;
        if (!radio_.readRegister(Radio::FIFO_STATUS, fifo)) return false;
        if (fifo & 0x01U) return true; // RX_EMPTY
        if (!radio_.readPayload(data) || !radio_.clearIrq(0x40)) return false;
        received=true;
        return true;
    }

private:
    Radio& radio_;
    Delay& delay_;
};
#endif
