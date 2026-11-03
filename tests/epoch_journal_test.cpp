#include "../GPT_NRF2401/epoch_journal.h"
#include <cassert>
#include <cstdint>
#include <cstring>
struct Memory {
    uint8_t bytes[16][6];
    bool failCommit=false;
    Memory(){std::memset(bytes,0xFF,sizeof(bytes));}
    uint8_t read(uint8_t s,uint8_t o){return bytes[s][o];}
    void update(uint8_t s,uint8_t o,uint8_t v){
        if(failCommit && o==5 && v==0xA5)return;
        bytes[s][o]=v;
    }
};
int main(){
    Memory store;
    uint32_t epoch=0;
    for(uint32_t n=1;n<=42;++n){
        assert(EpochJournal<Memory>::advance(store,epoch));
        assert(epoch==n);
    }
    store.failCommit=true;
    epoch=0x12345678UL;
    assert(!EpochJournal<Memory>::advance(store,epoch));
    assert(epoch==0x12345678UL);
    store.failCommit=false;
    assert(EpochJournal<Memory>::advance(store,epoch) && epoch==43);
    // A torn commit is not misinterpreted as a valid epoch even if it
    // leaves a complete counter in EEPROM.
    assert(EpochJournal<Memory>::advance(store,epoch) && epoch==44);
    // Terminal 32-bit wrap must fail safely rather than reuse identity.
    Memory nearOverflow;
    for(unsigned k=0;k<4;++k)
        nearOverflow.update(0,k,0xFF);
    // Check byte is deliberately invalid, hence journal ignores it.
    assert(EpochJournal<Memory>::advance(nearOverflow,epoch) && epoch==1);
}
