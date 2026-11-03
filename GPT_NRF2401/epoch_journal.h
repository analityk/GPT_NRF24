#ifndef GPT_NRF24_EPOCH_JOURNAL_H
#define GPT_NRF24_EPOCH_JOURNAL_H
#include <stdint.h>

// Wear-levelled monotonically increasing boot epoch. Storage adapter offers
// uint8_t read(slot,offset), void update(slot,offset,value).
// Commit is written last, leaving the previous record valid on torn writes.
// No heap and no assumptions about AVR EEPROM representation.
template<class Storage,uint8_t Slots=16>
class EpochJournal {
    static_assert(Slots>=2, "Crash-safe journal requires at least two slots");
public:
    static bool advance(Storage& store,uint32_t& value) {
        bool found=false;
        uint32_t highest=0;
        uint8_t last=0;
        for(uint8_t i=0;i<Slots;++i) {
            if(store.read(i,5)!=0xA5U)continue;
            uint32_t count=0;
            for(uint8_t k=0;k<4;++k)
                count|=static_cast<uint32_t>(store.read(i,k))<<(8U*k);
            if(check(count)!=store.read(i,4))continue;
            if(!found || count>highest){
                found=true;highest=count;last=i;
            }
        }
        if(found && highest==0xFFFFFFFFUL)return false;
        const uint8_t slot=found?static_cast<uint8_t>((last+1U)%Slots):0;
        const uint32_t next=found?highest+1UL:1UL;
        store.update(slot,5,0);
        for(uint8_t k=0;k<4;++k)
            store.update(slot,k,static_cast<uint8_t>(next>>(8U*k)));
        store.update(slot,4,check(next));
        store.update(slot,5,0xA5);
        uint32_t checkValue=0;
        for(uint8_t k=0;k<4;++k)
            checkValue|=static_cast<uint32_t>(store.read(slot,k))<<(8U*k);
        if(store.read(slot,5)!=0xA5U ||
           store.read(slot,4)!=check(next) ||
           checkValue!=next)return false;
        value=next;
        return true;
    }
private:
    static uint8_t check(uint32_t value) {
        uint8_t crc=0x69;
        for(uint8_t k=0;k<4;++k) {
            crc^=static_cast<uint8_t>(value>>(8U*k));
            for(uint8_t bit=0;bit<8;++bit)
                crc=static_cast<uint8_t>((crc&0x80U)?
                    ((crc<<1)^0x07U):(crc<<1));
        }
        return crc;
    }
};
#endif
