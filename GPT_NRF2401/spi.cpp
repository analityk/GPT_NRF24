#include "spi.h"
#include "spi_block_transfer.h"
#include "board_clock.h"
#include <avr/io.h>

namespace {
	struct AvrSpiBus {
		inline bool master() const{
			return (SPCR & _BV(MSTR))!=0;
		}

		inline void write(uint8_t value){
			SPDR=value;
		}

		inline bool ready() const{
			return (SPSR & _BV(SPIF))!=0;
		}

		inline uint8_t read() const{
			return SPDR;
		}
	};
}

void SpiMaster::begin(){
	// PB2 is hardware SS, wired to radio CE on this board. It must
	// remain OUTPUT even though radio CSN uses PB1.
	DDRB|=_BV(DDB2)|_BV(DDB3)|_BV(DDB5);
	DDRB&=static_cast<uint8_t>(~_BV(DDB4));
	SPCR=_BV(SPE)|_BV(MSTR)|_BV(SPR0);
	SPSR=0;
}

void SpiMaster::setSpeed(Speed speed){
	if(speed==Speed::MHz8){
		SPCR=_BV(SPE)|_BV(MSTR);
		SPSR=_BV(SPI2X);
	} else {
		SPCR=_BV(SPE)|_BV(MSTR)|_BV(SPR0);
		SPSR=0;
	}
}

bool SpiMaster::transfer(uint8_t outgoing,uint8_t& incoming){
	return transferBlock(outgoing,0,0,0,&incoming);
}

bool SpiMaster::transferBlock(uint8_t command,const uint8_t* tx,
	uint8_t* rx,uint8_t count,uint8_t* statusByte){
	AvrSpiBus bus;
	if(!SpiBlockTransfer<AvrSpiBus>::exchange(
		bus,command,tx,rx,count,statusByte)){
		++timeouts_;
		return false;
	}
	return true;
}
