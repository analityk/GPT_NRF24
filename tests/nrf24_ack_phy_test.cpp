#include "../GPT_NRF2401/nrf24_ack_phy.h"
#include <cassert>
#include <cstdint>
#include <cstring>

struct FakeClock {
    uint32_t ticks=0;
    uint32_t now() { ticks+=200; return ticks; }
};
struct FakeDelay {
    unsigned milli=0,micro=0;
    void ms(uint8_t n) {milli+=n;}
    void us(uint16_t n) {micro+=n;}
};
struct FakeRadio {
    enum {CONFIG=0,FIFO_STATUS=0x17,OBSERVE_TX=8};
    uint8_t config=0;
    uint8_t payload[32]={},reply[32]={};
    uint8_t width=32;
    bool gotPayload=false,rxPending=false,txFull=false,fail=false;
    bool ce=false;
    uint8_t txOutcome=0x20,ackArc=2;
    unsigned polls=0,flash=0,queued=0,clears=0,txPulse=0;
    void ceLow() {ce=false;}
    void ceHigh() {ce=true;++txPulse;}
    bool writeRegister(uint8_t reg,uint8_t val) {
        if(fail)return false;
        if(reg==CONFIG)config=val;
        return true;
    }
    bool readRegister(uint8_t reg,uint8_t& val) {
        if(fail)return false;
        if(reg==FIFO_STATUS)val=static_cast<uint8_t>((rxPending?0:1)|
                                                      (txFull?0x20:0));
        else if(reg==OBSERVE_TX)val=ackArc;
        else val=0;
        return true;
    }
    bool clearIrq(uint8_t) {++clears;return !fail;}
    bool writePayload(const uint8_t* p) {
        if(fail)return false;
        std::memcpy(payload,p,32);
        return true;
    }
    bool readDynamicPayload(uint8_t* p,uint8_t& n) {
        if(fail)return false;
        n=width;
        if(n>32 || n==0)return false;
        std::memcpy(p,reply,n);
        rxPending=false;
        return true;
    }
    bool writeAckPayload(uint8_t pipe,const uint8_t* p,uint8_t n) {
        if(fail)return false;
        assert(pipe==0 && n==32);
        std::memcpy(reply,p,n);
        ++queued;
        return true;
    }
    bool status(uint8_t& val) {
        if(fail)return false;
        ++polls;
        val=polls>=3?txOutcome:0;
        return true;
    }
    bool flushTx() {++flash;return !fail;}
};
int main() {
    FakeRadio hw;
    FakeClock clock;
    FakeDelay delay;
    typedef NrfAckPayloadPhy<FakeRadio,FakeDelay,FakeClock> Phy;
    Phy phy(hw,delay,clock);
    assert(phy.beginPtx());
    assert(hw.config==0x0E && delay.milli==2 && !hw.ce);
    uint8_t data[32]={},reply[32]={};
    for(unsigned i=0;i<32;++i) {
        data[i]=static_cast<uint8_t>(i);
        hw.reply[i]=static_cast<uint8_t>(255-i);
    }
    hw.rxPending=true;
    Phy::TxResult r=phy.transfer(data,reply);
    assert(r.code==Phy::TxCode::ACK_WITH_DATA && r.retries==2);
    assert(hw.polls>=3 && hw.txPulse==1 && !hw.ce);
    assert(!std::memcmp(reply,hw.reply,32));
    assert(delay.micro==15);

    hw.polls=0;hw.txOutcome=0x20;hw.rxPending=false;
    r=phy.transfer(data,reply);
    assert(r.code==Phy::TxCode::ACK_EMPTY);
    hw.polls=0;hw.rxPending=false;hw.txOutcome=0x10;
    r=phy.transfer(data,reply);
    assert(r.code==Phy::TxCode::MAX_RT && hw.flash==1);
    hw.polls=0;hw.txOutcome=0;
    r=phy.transfer(data,reply);
    assert(r.code==Phy::TxCode::TIMEOUT && hw.flash==2);
    hw.txOutcome=0x20;hw.polls=0;hw.rxPending=true;hw.width=31;
    r=phy.transfer(data,reply);
    assert(r.code==Phy::TxCode::BAD_REPLY_SIZE);

    FakeRadio rxHw;
    FakeDelay rxDelay;
    FakeClock rxClock;
    Phy receiver(rxHw,rxDelay,rxClock);
    assert(receiver.beginPrx());
    assert(rxHw.config==0x0F && rxHw.ce && rxDelay.milli==2 &&
           rxDelay.micro==150);
    assert(receiver.queueReply(data)==Phy::QueueCode::QUEUED);
    assert(rxHw.queued==1);
    rxHw.txFull=true;
    assert(receiver.queueReply(data)==Phy::QueueCode::FULL);
    rxHw.txFull=false;
    assert(receiver.receive(reply)==Phy::RxCode::EMPTY);
    rxHw.rxPending=true;
    assert(receiver.receive(reply)==Phy::RxCode::GOOD);
    rxHw.rxPending=true;rxHw.width=12;
    assert(receiver.receive(reply)==Phy::RxCode::BAD_SIZE);
    rxHw.fail=true;
    assert(receiver.receive(reply)==Phy::RxCode::IO_ERROR);
    assert(receiver.queueReply(data)==Phy::QueueCode::IO_ERROR);
}
