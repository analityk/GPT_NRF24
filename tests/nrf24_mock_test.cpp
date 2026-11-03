#include "../GPT_NRF2401/nrf24.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

struct SpiMock {
    bool selected=false;
    bool fail=false;
    bool stuckChannel=false;
    bool disconnected=false;
    bool initialized=false;
    uint8_t command=0;
    uint8_t position=0;
    uint8_t regs[32]={};
    uint8_t rxAddress[5]={};
    uint8_t txAddress[5]={};
    uint8_t rxPayload[32]={};
    uint8_t txPayload[32]={};
    uint8_t txPayloadCount=0;
    unsigned flushCount=0;

    SpiMock() {
        regs[0x05]=2; // RF_CH reset
        regs[0x07]=0x0E; // STATUS reset
        regs[0x17]=0x11; // FIFO_STATUS reset
    }
    void begin() { initialized=true; }
    bool transfer(uint8_t out, uint8_t& in) {
        if (fail || !selected || !initialized) return false;
        if (disconnected) { in=0xFF; return true; }
        if (position++ == 0) {
            command=out;
            in=regs[0x07];
            if (command==0xE1 || command==0xE2) ++flushCount;
            return true;
        }
        const uint8_t index=static_cast<uint8_t>(position-2);
        const uint8_t reg=static_cast<uint8_t>(command & 0x1F);
        if (command==0xA0) {
            txPayload[index]=out;
            ++txPayloadCount;
            in=0;
        } else if (command==0x61) {
            in=rxPayload[index];
        } else if ((command & 0xE0U)==0x20U) {
            if (reg==0x0A && index<5) rxAddress[index]=out;
            else if (reg==0x10 && index<5) txAddress[index]=out;
            else if (reg==0x07) regs[reg] &= static_cast<uint8_t>(~(out & 0x70U));
            else if (reg!=0x05 || !stuckChannel) regs[reg]=out;
            in=0;
        } else if ((command & 0xE0U)==0U) {
            if (reg==0x0A && index<5) in=rxAddress[index];
            else if (reg==0x10 && index<5) in=txAddress[index];
            else in=regs[reg];
        } else in=0;
        return true;
    }
};
struct PinMock {
    SpiMock& spi;
    bool csn;
    bool high=false;
    PinMock(SpiMock& s, bool isCsn) : spi(s), csn(isCsn) {}
    void clear() {
        high=false;
        if (csn) { spi.selected=true; spi.position=0; }
    }
    void set() {
        high=true;
        if (csn) spi.selected=false;
    }
};
int main() {
    SpiMock hw;
    PinMock ce(hw,false), csn(hw,true);
    Nrf24<SpiMock,PinMock> radio(hw,ce,csn);
    const auto result=radio.probeAndConfigure();
    assert(result.result==Nrf24<SpiMock,PinMock>::Result::OK);
    assert(hw.initialized && !hw.selected && csn.high && !ce.high);
    assert(hw.regs[0x00]==0x0C && hw.regs[0x01]==1 &&
           hw.regs[0x02]==1 && hw.regs[0x03]==3);
    assert(hw.regs[0x04]==0x3F && hw.regs[0x05]==76 &&
           hw.regs[0x06]==6 && hw.regs[0x11]==32);
    assert(hw.regs[0x1C]==0 && hw.regs[0x1D]==0);
    assert(hw.flushCount==2);
    assert(hw.rxAddress[0]==0xD2 && hw.rxAddress[4]==0xA1);
    assert(memcmp(hw.rxAddress,hw.txAddress,5)==0);
    uint8_t values[32]={};
    for (unsigned i=0;i<32;++i) values[i]=static_cast<uint8_t>(i*3U);
    assert(radio.writePayload(values));
    assert(hw.txPayloadCount==32 && memcmp(values,hw.txPayload,32)==0);
    memcpy(hw.rxPayload,values,32);
    memset(values,0,sizeof(values));
    assert(radio.readPayload(values));
    assert(memcmp(values,hw.rxPayload,32)==0);
    assert(!radio.writePayload(nullptr));
    assert(!radio.readPayload(nullptr));
    assert(!radio.readRegisters(0x05,values,0));
    assert(!radio.writeRegisters(0x05,values,6));
    uint8_t s=0;
    assert(radio.status(s) && s==0x0E);

    SpiMock missing; missing.disconnected=true;
    PinMock mCe(missing,false), mCsn(missing,true);
    Nrf24<SpiMock,PinMock> absent(missing,mCe,mCsn);
    assert(absent.probeAndConfigure().result==Nrf24<SpiMock,PinMock>::Result::NO_RADIO);

    SpiMock stuck; stuck.stuckChannel=true;
    PinMock sCe(stuck,false), sCsn(stuck,true);
    Nrf24<SpiMock,PinMock> wrong(stuck,sCe,sCsn);
    assert(wrong.probeAndConfigure().result==Nrf24<SpiMock,PinMock>::Result::VERIFY_ERROR);
    assert(stuck.regs[0x05]==2); // original RF_CH preserved
    assert(!stuck.selected && sCsn.high);

    SpiMock badSpi; badSpi.fail=true;
    PinMock bCe(badSpi,false), bCsn(badSpi,true);
    Nrf24<SpiMock,PinMock> failed(badSpi,bCe,bCsn);
    assert(failed.probeAndConfigure().result==Nrf24<SpiMock,PinMock>::Result::SPI_ERROR);
    assert(!badSpi.selected && bCsn.high);
}
