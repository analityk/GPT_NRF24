#include "../GPT_NRF2401/nrf24_ack_phy.h"
#include <cassert>
#include <cstdint>
#include <cstring>

#define PP_ASYNC_TEST_REPLY_BYTES NRF24_PHY_MIN_FRAME_BYTES
#define PP_ASYNC_TEST_OVERFLOW_START 0xFFFFFF00UL
#define PP_ASYNC_TEST_LAST_TICK 1UL
#define PP_ASYNC_TEST_RETRIES 2U
#define PP_ASYNC_TEST_MUTATED_PAYLOAD_BYTE 0xA5U
#define PP_ASYNC_TEST_CONFIG_REGISTER 0x00U
#define PP_ASYNC_TEST_FIFO_REGISTER 0x17U
#define PP_ASYNC_TEST_OBSERVE_REGISTER 0x08U

struct TestClock {
    uint32_t ticks=0;
    uint32_t now()const{return ticks;}
    void advance(uint32_t n){ticks+=n;}
};
struct TestDelay {
    void ms(uint8_t){}
    void us(uint16_t){}
};
struct TestRadio {
    enum { CONFIG=PP_ASYNC_TEST_CONFIG_REGISTER,
           FIFO_STATUS=PP_ASYNC_TEST_FIFO_REGISTER,
           OBSERVE_TX=PP_ASYNC_TEST_OBSERVE_REGISTER };
    uint8_t tx[NRF24_PHY_MAX_FRAME_BYTES]={};
    uint8_t rx[NRF24_PHY_MAX_FRAME_BYTES]={};
    uint8_t width=PP_ASYNC_TEST_REPLY_BYTES;
    uint8_t irq=0;
    bool ce=false,rxPresent=false,failStatus=false;
    unsigned statusReads=0,cePulses=0,txFlushes=0,rxFlushes=0;
    void ceLow(){ce=false;}
    void ceHigh(){ce=true;++cePulses;}
    bool writeRegister(uint8_t,uint8_t){return true;}
    bool readRegister(uint8_t reg,uint8_t& value){
        if(reg==FIFO_STATUS)
            value=rxPresent?0:NRF24_PHY_RX_EMPTY_MASK;
        else if(reg==OBSERVE_TX)value=PP_ASYNC_TEST_RETRIES;
        return true;
    }
    bool clearIrq(uint8_t){irq=0;return true;}
    bool writeDynamicPayload(const uint8_t* bytes,uint8_t n){
        std::memcpy(tx,bytes,n);
        return true;
    }
    bool status(uint8_t& value){
        ++statusReads;
        if(failStatus)return false;
        value=irq;
        return true;
    }
    bool readDynamicPayload(uint8_t* bytes,uint8_t& n){
        n=width;
        if(n>NRF24_PHY_MAX_FRAME_BYTES || !n)return false;
        std::memcpy(bytes,rx,n);
        rxPresent=false;
        return true;
    }
    bool writeAckPayload(uint8_t,const uint8_t*,uint8_t){return true;}
    bool flushTx(){++txFlushes;return true;}
    bool flushRx(){++rxFlushes;return true;}
};

typedef NrfAckPayloadPhy<TestRadio,TestDelay,TestClock> Phy;

static void pulse(Phy& phy,TestRadio& radio,TestClock& clock,
                  uint8_t (&rx)[NRF24_PHY_MAX_FRAME_BYTES],
                  uint8_t& width,Phy::TxResult& result) {
    const unsigned pollsBefore=radio.statusReads;
    assert(phy.pollTransfer(rx,width,result)==Phy::TxStep::PENDING);
    assert(phy.txPhase()==Phy::TxPhase::CE_PULSE && radio.ce);
    clock.advance(NRF24_PHY_CE_PULSE_TICKS-PP_ASYNC_TEST_LAST_TICK);
    assert(phy.pollTransfer(rx,width,result)==Phy::TxStep::PENDING);
    assert(radio.ce && radio.statusReads==pollsBefore);
    clock.advance(PP_ASYNC_TEST_LAST_TICK);
    assert(phy.pollTransfer(rx,width,result)==Phy::TxStep::PENDING);
    assert(phy.txOnAir() && !radio.ce &&
           radio.statusReads==pollsBefore);
}

int main() {
    TestClock clock;TestDelay delay;TestRadio radio;
    Phy phy(radio,delay,clock);
    assert(phy.beginPtx());
    uint8_t out[NRF24_PHY_MAX_FRAME_BYTES]={};
    uint8_t reply[NRF24_PHY_MAX_FRAME_BYTES]={};
    for(uint8_t i=0;i<PP_ASYNC_TEST_REPLY_BYTES;++i)
        radio.rx[i]=static_cast<uint8_t>(i+1U);
    uint8_t width=0;
    Phy::TxResult result;

    // The frame is already loaded when CE starts. The caller can reuse the
    // staging buffer while radio transmits; no additional 4x32B SRAM cache.
    assert(phy.startTransfer(out,NRF24_PHY_MAX_FRAME_BYTES));
    assert(!phy.startTransfer(out,NRF24_PHY_MAX_FRAME_BYTES));
    assert(radio.statusReads==0 && !radio.ce);
    pulse(phy,radio,clock,reply,width,result);
    out[0]=PP_ASYNC_TEST_MUTATED_PAYLOAD_BYTE;
    assert(radio.tx[0]!=out[0]);
    // Host work runs while the nRF is on air, without delaying CE or SPI.
    unsigned producerRuns=0;
    ++producerRuns;
    assert(phy.pollTransfer(reply,width,result)==Phy::TxStep::PENDING);
    assert(radio.statusReads==1U && producerRuns==1U);
    radio.rxPresent=true;
    radio.irq=NRF24_PHY_IRQ_TX_DONE_MASK;
    assert(phy.pollTransfer(reply,width,result)==Phy::TxStep::COMPLETE);
    assert(result.code==Phy::TxCode::ACK_WITH_DATA &&
           result.retries==PP_ASYNC_TEST_RETRIES &&
           width==PP_ASYNC_TEST_REPLY_BYTES);
    assert(!std::memcmp(reply,radio.rx,PP_ASYNC_TEST_REPLY_BYTES));
    assert(!phy.txBusy());

    // Guard waits without spinning, even if the clock crosses uint32 wrap.
    clock.ticks=PP_ASYNC_TEST_OVERFLOW_START;
    assert(phy.resetPtxFifos());
    assert(phy.startTransfer(out,NRF24_PHY_MIN_FRAME_BYTES));
    const unsigned pulsesBefore=radio.cePulses;
    assert(phy.pollTransfer(reply,width,result)==Phy::TxStep::PENDING);
    clock.advance(NRF24_PHY_PTX_GUARD_TICKS-PP_ASYNC_TEST_LAST_TICK);
    assert(phy.pollTransfer(reply,width,result)==Phy::TxStep::PENDING);
    assert(radio.cePulses==pulsesBefore);
    clock.advance(PP_ASYNC_TEST_LAST_TICK);
    pulse(phy,radio,clock,reply,width,result);
    radio.irq=0;
    const unsigned beforeStatus=radio.statusReads;
    assert(phy.pollTransfer(reply,width,result)==Phy::TxStep::PENDING);
    assert(radio.statusReads==beforeStatus+1U);
    clock.advance(NRF24_PHY_TX_TIMEOUT_TICKS-PP_ASYNC_TEST_LAST_TICK);
    assert(phy.pollTransfer(reply,width,result)==Phy::TxStep::PENDING);
    clock.advance(PP_ASYNC_TEST_LAST_TICK);
    assert(phy.pollTransfer(reply,width,result)==Phy::TxStep::COMPLETE);
    assert(result.code==Phy::TxCode::TIMEOUT && !phy.txBusy());
    assert(radio.txFlushes>=1U);

    // Interrupted SPI status read terminates the state machine. The next
    // packet can be scheduled after the upper layer has reset its FIFO.
    assert(phy.resetPtxFifos());
    assert(phy.startTransfer(out,NRF24_PHY_MIN_FRAME_BYTES));
    clock.advance(NRF24_PHY_PTX_GUARD_TICKS);
    pulse(phy,radio,clock,reply,width,result);
    radio.failStatus=true;
    assert(phy.pollTransfer(reply,width,result)==Phy::TxStep::COMPLETE);
    assert(result.code==Phy::TxCode::IO_ERROR && !phy.txBusy());
    assert(phy.pollTransfer(reply,width,result)==Phy::TxStep::IDLE);
}
