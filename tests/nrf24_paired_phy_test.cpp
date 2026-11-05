#include "../GPT_NRF2401/nrf24_ack_phy.h"
#include <cassert>
#include <cstdint>
#include <cstring>

#define PHY_TEST_CLOCK_STEP_TICKS 100U
#define PHY_TEST_OBSERVED_RETRIES 2U
#define PHY_TEST_TERMINAL_POLL_COUNT 2U
#define PHY_TEST_BAD_SHORT_WIDTH (NRF24_PHY_MIN_FRAME_BYTES-1U)
#define PHY_TEST_BAD_LONG_WIDTH (NRF24_PHY_MAX_FRAME_BYTES+1U)
#define PHY_TEST_FAKE_CONFIG 0x00U
#define PHY_TEST_FAKE_FIFO_STATUS 0x17U
#define PHY_TEST_FAKE_OBSERVE_TX 0x08U
#define PHY_TEST_ONE_COUNT 1U
#define PHY_TEST_REKEY_GUARD_TOTAL_US (NRF24_PHY_PRX_REKEY_GUARD_US+NRF24_PHY_PRX_SETTLE_US)

struct Clock {
    uint32_t tick=0;
    uint32_t now(){tick+=PHY_TEST_CLOCK_STEP_TICKS;return tick;}
};
struct Delay {
    unsigned msTotal=0,usTotal=0;
    void ms(uint8_t v){msTotal+=v;}
    void us(uint16_t v){usTotal+=v;}
};
struct Fake {
    enum {CONFIG=PHY_TEST_FAKE_CONFIG,FIFO_STATUS=PHY_TEST_FAKE_FIFO_STATUS,
          OBSERVE_TX=PHY_TEST_FAKE_OBSERVE_TX};
    uint8_t tx[NRF24_PHY_MAX_FRAME_BYTES]={};
    uint8_t rx[NRF24_PHY_MAX_FRAME_BYTES]={};
    uint8_t txWidth=0,rxWidth=NRF24_PHY_MIN_FRAME_BYTES,ackWidth=0;
    bool rxPresent=false,rxFifoFull=false,full=false,fail=false;
    uint8_t txFlags=NRF24_PHY_IRQ_TX_DONE_MASK;
    unsigned polls=0,highs=0,flushes=0,rxFlushes=0;
    void ceLow() {}
    void ceHigh(){++highs;}
    bool writeRegister(uint8_t,uint8_t){return !fail;}
    bool readRegister(uint8_t r,uint8_t& out){
        if(fail)return false;
        if(r==FIFO_STATUS)
            out=static_cast<uint8_t>((rxPresent?0:NRF24_PHY_RX_EMPTY_MASK)|
                                      (rxFifoFull?NRF24_PHY_RX_FULL_MASK:0)|
                                      (full?NRF24_PHY_TX_FULL_MASK:0));
        else if(r==OBSERVE_TX)out=PHY_TEST_OBSERVED_RETRIES;
        return true;
    }
    bool clearIrq(uint8_t){return !fail;}
    bool status(uint8_t& s){
        if(fail)return false;
        s=(++polls>=PHY_TEST_TERMINAL_POLL_COUNT)?txFlags:0;
        return true;
    }
    bool writeDynamicPayload(const uint8_t* p,uint8_t n){
        if(fail)return false;
        txWidth=n;std::memcpy(tx,p,n);
        return true;
    }
    bool readDynamicPayload(uint8_t* p,uint8_t& n){
        if(fail)return false;
        n=rxWidth;
        if(!n || n>NRF24_PHY_MAX_FRAME_BYTES)return false;
        std::memcpy(p,rx,n);rxPresent=false;
        return true;
    }
    bool writeAckPayload(uint8_t pipe,const uint8_t* p,uint8_t n){
        if(fail)return false;
        assert(pipe==NRF24_PHY_ACK_PIPE);ackWidth=n;
        std::memcpy(rx,p,n);
        return true;
    }
    bool flushTx(){++flushes;return !fail;}
    bool flushRx(){++rxFlushes;return !fail;}
};

#define TEST_PHY_MAX_POLL_STEPS 512U

typedef NrfAckPayloadPhy<Fake,Delay,Clock> Phy;
static Phy::TxResult exchange(Phy& phy,Fake& radio,Clock&,
                              const uint8_t (&out)[NRF24_PHY_MAX_FRAME_BYTES],
                              uint8_t txWidth,
                              uint8_t (&reply)[NRF24_PHY_MAX_FRAME_BYTES],
                              uint8_t& width) {
    Phy::TxResult result;
    width=0;
    assert(phy.startTransfer(out,txWidth));
    assert(phy.txBusy());
    // The mock CPU makes forward progress on every step; the production
    // scheduler is free to run unrelated work while the chip is on air.
    for(unsigned i=0;i<TEST_PHY_MAX_POLL_STEPS;++i) {
        const Phy::TxStep step=phy.pollTransfer(reply,width,result);
        if(step==Phy::TxStep::COMPLETE) {
            assert(!phy.txBusy());
            return result;
        }
        assert(step==Phy::TxStep::PENDING);
    }
    assert(false && "PHY state machine did not terminate");
    (void)radio;
    return result;
}
int main() {
    Fake radio;Delay delay;Clock clock;
    Phy phy(radio,delay,clock);
    assert(phy.beginPtx());
    uint8_t out[NRF24_PHY_MAX_FRAME_BYTES]={};
    uint8_t back[NRF24_PHY_MAX_FRAME_BYTES]={},width=0;
    for(unsigned i=0;i<NRF24_PHY_MAX_FRAME_BYTES;++i){
        out[i]=static_cast<uint8_t>(i);
        radio.rx[i]=static_cast<uint8_t>(i+1);
    }
    radio.rxPresent=true;radio.rxWidth=NRF24_PHY_MIN_FRAME_BYTES;
    auto r=exchange(phy,radio,clock,out,NRF24_PHY_MIN_FRAME_BYTES,back,width);
    assert(r.code==Phy::TxCode::ACK_WITH_DATA &&
           width==NRF24_PHY_MIN_FRAME_BYTES &&
           radio.txWidth==NRF24_PHY_MIN_FRAME_BYTES);
    assert(!std::memcmp(back,radio.rx,NRF24_PHY_MIN_FRAME_BYTES));
    assert(r.retries==PHY_TEST_OBSERVED_RETRIES &&
           radio.highs==PHY_TEST_ONE_COUNT);
    radio.polls=0;radio.rxPresent=true;radio.rxWidth=NRF24_PHY_MAX_FRAME_BYTES;
    r=exchange(phy,radio,clock,out,NRF24_PHY_MAX_FRAME_BYTES,back,width);
    assert(r.code==Phy::TxCode::ACK_WITH_DATA &&
           width==NRF24_PHY_MAX_FRAME_BYTES &&
           radio.txWidth==NRF24_PHY_MAX_FRAME_BYTES);
    assert(!std::memcmp(back,radio.rx,NRF24_PHY_MAX_FRAME_BYTES));
    radio.polls=0;radio.rxPresent=false;
    r=exchange(phy,radio,clock,out,NRF24_PHY_MIN_FRAME_BYTES,back,width);
    assert(r.code==Phy::TxCode::ACK_EMPTY && width==0);
    radio.polls=0;radio.rxPresent=true;radio.rxWidth=PHY_TEST_BAD_SHORT_WIDTH;
    r=exchange(phy,radio,clock,out,NRF24_PHY_MIN_FRAME_BYTES,back,width);
    assert(r.code==Phy::TxCode::BAD_REPLY_SIZE &&
           width==PHY_TEST_BAD_SHORT_WIDTH);
    radio.polls=0;radio.txFlags=NRF24_PHY_IRQ_MAX_RT_MASK;
    r=exchange(phy,radio,clock,out,NRF24_PHY_MIN_FRAME_BYTES,back,width);
    assert(r.code==Phy::TxCode::MAX_RT &&
           radio.flushes==PHY_TEST_ONE_COUNT);
    radio.polls=0;radio.txFlags=0;
    r=exchange(phy,radio,clock,out,NRF24_PHY_MIN_FRAME_BYTES,back,width);
    assert(r.code==Phy::TxCode::TIMEOUT &&
           radio.flushes==PHY_TEST_ONE_COUNT+PHY_TEST_ONE_COUNT);
    assert(!phy.startTransfer(out,PHY_TEST_BAD_SHORT_WIDTH));
    assert(!phy.txBusy());
    Fake receiver;Delay rxDelay;Clock rxClock;
    Phy prx(receiver,rxDelay,rxClock);
    assert(prx.beginPrx());
    assert(prx.queueReply(out,NRF24_PHY_MIN_FRAME_BYTES)==
           Phy::QueueCode::QUEUED &&
           receiver.ackWidth==NRF24_PHY_MIN_FRAME_BYTES);
    assert(prx.queueReply(out,NRF24_PHY_MAX_FRAME_BYTES)==
           Phy::QueueCode::QUEUED &&
           receiver.ackWidth==NRF24_PHY_MAX_FRAME_BYTES);
    assert(prx.queueReply(out,PHY_TEST_BAD_SHORT_WIDTH)==Phy::QueueCode::IO_ERROR);
    receiver.full=true;
    assert(prx.queueReply(out,NRF24_PHY_MAX_FRAME_BYTES)==Phy::QueueCode::FULL);
    receiver.full=false;
    assert(prx.receive(back,width)==Phy::RxCode::EMPTY && width==0);
    assert(prx.rxFifoFullSamples()==0);
    receiver.rxFifoFull=true;
    receiver.rxPresent=true;receiver.rxWidth=NRF24_PHY_MIN_FRAME_BYTES;
    assert(prx.receive(back,width)==Phy::RxCode::GOOD &&
           width==NRF24_PHY_MIN_FRAME_BYTES);
    assert(prx.rxFifoFullSamples()==PHY_TEST_ONE_COUNT);
    receiver.rxFifoFull=false;
    receiver.rxPresent=true;receiver.rxWidth=PHY_TEST_BAD_SHORT_WIDTH;
    assert(prx.receive(back,width)==Phy::RxCode::BAD_SIZE &&
           width==PHY_TEST_BAD_SHORT_WIDTH);
    receiver.rxPresent=true;receiver.rxWidth=PHY_TEST_BAD_LONG_WIDTH;
    assert(prx.receive(back,width)==Phy::RxCode::BAD_SIZE &&
           width==PHY_TEST_BAD_LONG_WIDTH);
    // Re-keying must discard both RX and preloaded ACK TX FIFO.
    receiver.full=false;
    assert(prx.replacePrxReply(out,NRF24_PHY_MIN_FRAME_BYTES));
    assert(receiver.flushes==PHY_TEST_ONE_COUNT &&
           receiver.rxFlushes==PHY_TEST_ONE_COUNT &&
           receiver.ackWidth==NRF24_PHY_MIN_FRAME_BYTES);
    assert(rxDelay.usTotal>=PHY_TEST_REKEY_GUARD_TOTAL_US);
    assert(!prx.replacePrxReply(out,PHY_TEST_BAD_SHORT_WIDTH));
    // A may discard stale packets without powering its oscillator down.
    assert(phy.resetPtxFifos());
    assert(radio.flushes==PHY_TEST_ONE_COUNT+PHY_TEST_ONE_COUNT+
                           PHY_TEST_ONE_COUNT &&
           radio.rxFlushes==PHY_TEST_ONE_COUNT);
    receiver.fail=true;
    assert(prx.receive(back,width)==Phy::RxCode::IO_ERROR);
}
