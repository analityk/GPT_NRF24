#include "../GPT_NRF2401/nrf24_phy.h"
#include <cassert>
#include <cstdint>
#include <cstring>

struct FakeDelay {
    unsigned micro=0, milli=0;
    void us(uint16_t us) {micro+=us;}
    void ms(uint8_t ms) {milli+=ms;}
};

struct FakeRadio {
    enum {CONFIG=0, FIFO_STATUS=0x17, OBSERVE_TX=8};
    uint8_t config=0, flags=0, observe=0x03;
    uint8_t frame[32]={};
    uint8_t rxFrame[32]={};
    bool ce=false, rxEmpty=true, fail=false;
    unsigned highCount=0, lowCount=0, flushCount=0, clearCount=0, polls=0;
    bool writeRegister(uint8_t r,uint8_t v) {
        if (fail) return false;
        if (r==CONFIG) config=v;
        return true;
    }
    bool readRegister(uint8_t r,uint8_t& v) {
        if (fail) return false;
        if (r==OBSERVE_TX) v=observe;
        else if (r==FIFO_STATUS) v=rxEmpty ? 1:0;
        return true;
    }
    bool flushTx() {++flushCount; return !fail;}
    bool clearIrq(uint8_t) {++clearCount; return !fail;}
    bool writePayload(const uint8_t* p) {
        if (fail) return false;
        memcpy(frame,p,32);
        return true;
    }
    bool readPayload(uint8_t* p) {
        if (fail) return false;
        memcpy(p,rxFrame,32);
        rxEmpty=true;
        return true;
    }
    bool status(uint8_t& s) {
        if (fail) return false;
        ++polls; s=flags; return true;
    }
    void ceHigh() {ce=true; ++highCount;}
    void ceLow() {ce=false; ++lowCount;}
};

int main() {
    FakeRadio r;
    FakeDelay delay;
    Nrf24Phy<FakeRadio,FakeDelay> phy(r,delay);
    uint8_t payload[32]={};
    for (unsigned i=0; i<32; ++i) payload[i]=static_cast<uint8_t>(i);
    assert(phy.listen());
    assert(r.ce && r.config==0x0F && delay.milli==2 && delay.micro==150);
    phy.standby();
    assert(!r.ce);
    bool received=true;
    assert(phy.receive(payload,received) && !received);
    r.rxEmpty=false;
    for (int i=0; i<32; ++i)r.rxFrame[i]=static_cast<uint8_t>(31-i);
    assert(phy.receive(payload,received) && received);
    assert(payload[0]==31 && payload[31]==0 && r.rxEmpty);
    r.flags=0x20;
    r.polls=0;
    uint8_t outgoing[32]={};
    outgoing[0]=0xA5;
    const auto ok=phy.transmit(outgoing);
    assert(ok.result==decltype(ok.result)::ACKED && ok.arc==3);
    assert(r.polls==1 && r.frame[0]==0xA5 && r.config==0x0E && !r.ce);
    r.flags=0x10;
    const auto lost=phy.transmit(outgoing);
    assert(lost.result==decltype(lost.result)::MAX_RETRIES);
    assert(r.flushCount>=3);
    r.flags=0;
    const unsigned previousMs=delay.milli;
    const auto timeout=phy.transmit(outgoing);
    assert(timeout.result==decltype(timeout.result)::TIMEOUT);
    assert(delay.milli-previousMs>=32);
    r.fail=true;
    const auto io=phy.transmit(outgoing);
    assert(io.result==decltype(io.result)::IO_ERROR && !r.ce);
    assert(!phy.receive(payload,received));
    assert(!phy.listen());
}
