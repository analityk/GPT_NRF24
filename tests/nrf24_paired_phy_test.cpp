#include "../GPT_NRF2401/nrf24_ack_phy.h"
#include <cassert>
#include <cstdint>
#include <cstring>

struct Clock {
    uint32_t tick=0;
    uint32_t now(){tick+=100;return tick;}
};
struct Delay {
    unsigned msTotal=0,usTotal=0;
    void ms(uint8_t v){msTotal+=v;}
    void us(uint16_t v){usTotal+=v;}
};
struct Fake {
    enum {CONFIG=0,FIFO_STATUS=0x17,OBSERVE_TX=8};
    uint8_t tx[32]={},rx[32]={};
    uint8_t txWidth=0,rxWidth=8,ackWidth=0;
    bool rxPresent=false,rxFifoFull=false,full=false,fail=false;
    uint8_t txFlags=0x20;
    unsigned polls=0,highs=0,flushes=0,rxFlushes=0;
    void ceLow() {}
    void ceHigh(){++highs;}
    bool writeRegister(uint8_t,uint8_t){return !fail;}
    bool readRegister(uint8_t r,uint8_t& out){
        if(fail)return false;
        if(r==FIFO_STATUS)
            out=static_cast<uint8_t>((rxPresent?0:1)|
                                      (rxFifoFull?0x02:0)|
                                      (full?0x20:0));
        else if(r==OBSERVE_TX)out=2;
        return true;
    }
    bool clearIrq(uint8_t){return !fail;}
    bool status(uint8_t& s){
        if(fail)return false;
        s=(++polls>=2)?txFlags:0;
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
        if(!n || n>32)return false;
        std::memcpy(p,rx,n);rxPresent=false;
        return true;
    }
    bool writeAckPayload(uint8_t pipe,const uint8_t* p,uint8_t n){
        if(fail)return false;
        assert(pipe==0);ackWidth=n;
        std::memcpy(rx,p,n);
        return true;
    }
    bool flushTx(){++flushes;return !fail;}
    bool flushRx(){++rxFlushes;return !fail;}
};
int main() {
    typedef NrfAckPayloadPhy<Fake,Delay,Clock> Phy;
    Fake radio;Delay delay;Clock clock;
    Phy phy(radio,delay,clock);
    assert(phy.beginPtx());
    uint8_t out[32]={},back[32]={},width=0;
    for(unsigned i=0;i<32;++i){
        out[i]=static_cast<uint8_t>(i);
        radio.rx[i]=static_cast<uint8_t>(i+1);
    }
    radio.rxPresent=true;radio.rxWidth=8;
    auto r=phy.transfer(out,8,back,width);
    assert(r.code==Phy::TxCode::ACK_WITH_DATA && width==8 && radio.txWidth==8);
    assert(!std::memcmp(back,radio.rx,8));
    assert(r.retries==2 && radio.highs==1);
    radio.polls=0;radio.rxPresent=true;radio.rxWidth=32;
    r=phy.transfer(out,32,back,width);
    assert(r.code==Phy::TxCode::ACK_WITH_DATA && width==32 && radio.txWidth==32);
    assert(!std::memcmp(back,radio.rx,32));
    radio.polls=0;radio.rxPresent=false;
    r=phy.transfer(out,8,back,width);
    assert(r.code==Phy::TxCode::ACK_EMPTY && width==0);
    radio.polls=0;radio.rxPresent=true;radio.rxWidth=7;
    r=phy.transfer(out,8,back,width);
    assert(r.code==Phy::TxCode::BAD_REPLY_SIZE && width==7);
    radio.polls=0;radio.txFlags=0x10;
    r=phy.transfer(out,8,back,width);
    assert(r.code==Phy::TxCode::MAX_RT && radio.flushes==1);
    radio.polls=0;radio.txFlags=0;
    r=phy.transfer(out,8,back,width);
    assert(r.code==Phy::TxCode::TIMEOUT && radio.flushes==2);
    r=phy.transfer(out,7,back,width);
    assert(r.code==Phy::TxCode::IO_ERROR && width==0);
    Fake receiver;Delay rxDelay;Clock rxClock;
    Phy prx(receiver,rxDelay,rxClock);
    assert(prx.beginPrx());
    assert(prx.queueReply(out,8)==Phy::QueueCode::QUEUED && receiver.ackWidth==8);
    assert(prx.queueReply(out,32)==Phy::QueueCode::QUEUED && receiver.ackWidth==32);
    assert(prx.queueReply(out,7)==Phy::QueueCode::IO_ERROR);
    receiver.full=true;
    assert(prx.queueReply(out,32)==Phy::QueueCode::FULL);
    receiver.full=false;
    assert(prx.receive(back,width)==Phy::RxCode::EMPTY && width==0);
    assert(prx.rxFifoFullSamples()==0);
    receiver.rxFifoFull=true;
    receiver.rxPresent=true;receiver.rxWidth=8;
    assert(prx.receive(back,width)==Phy::RxCode::GOOD && width==8);
    assert(prx.rxFifoFullSamples()==1);
    receiver.rxFifoFull=false;
    receiver.rxPresent=true;receiver.rxWidth=7;
    assert(prx.receive(back,width)==Phy::RxCode::BAD_SIZE && width==7);
    receiver.rxPresent=true;receiver.rxWidth=33;
    assert(prx.receive(back,width)==Phy::RxCode::BAD_SIZE && width==33);
    // Re-keying must discard both RX and preloaded ACK TX FIFO.
    receiver.full=false;
    assert(prx.replacePrxReply(out,8));
    assert(receiver.flushes==1 && receiver.rxFlushes==1 &&
           receiver.ackWidth==8);
    assert(rxDelay.usTotal>=700); // 550us guard + 150us PRX settle
    assert(!prx.replacePrxReply(out,7));
    // A may discard stale packets without powering its oscillator down.
    assert(phy.resetPtxFifos());
    assert(radio.flushes==3 && radio.rxFlushes==1);
    receiver.fail=true;
    assert(prx.receive(back,width)==Phy::RxCode::IO_ERROR);
}
