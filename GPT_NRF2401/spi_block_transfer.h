#ifndef GPT_NRF24_SPI_BLOCK_TRANSFER_H
#define GPT_NRF24_SPI_BLOCK_TRANSFER_H

#include <stdint.h>

// An 8-bit bounded poll is sufficient for a 1 MHz or 8 MHz SPI byte on
// a 16 MHz AVR, including normal UART interrupt latency. A stalled SPI
// peripheral still fails rather than hanging the RF state machine.
#define SPI_BLOCK_BYTE_TIMEOUT_POLLS 255U
#define SPI_BLOCK_READ_FILL_BYTE 0xFFU

// Reusable, allocation-free SPI block engine. Bus provides master(),
// write(), ready() and read(). Real AVR register access gets inlined;
// a fake Bus can verify the transaction, individual-byte timeout and
// receive sequence without an AVR or a radio.
template<class Bus>
class SpiBlockTransfer {
public:
	static bool exchange(Bus& bus,uint8_t command,const uint8_t* tx,
		uint8_t* rx,uint8_t count,uint8_t* statusByte){
		if(!bus.master()){
			return false;
		}
		uint8_t commandStatus=0;
		if(!shift(bus,command,commandStatus)){
			return false;
		}
		if(statusByte){
			*statusByte=commandStatus;
		}
		if(tx){
			if(rx){
				return data<true,true>(bus,tx,rx,count);
			}
			return data<true,false>(bus,tx,rx,count);
		}
		if(rx){
			return data<false,true>(bus,tx,rx,count);
		}
		return data<false,false>(bus,tx,rx,count);
	}

private:
	static inline __attribute__((always_inline)) bool shift(
		Bus& bus,uint8_t outgoing,uint8_t& incoming){
		bus.write(outgoing);
		uint8_t remaining=SPI_BLOCK_BYTE_TIMEOUT_POLLS;
		do {
			if(bus.ready()){
				// Reading AVR SPSR (in ready) then SPDR (here) clears SPIF.
				incoming=bus.read();
				return true;
			}
		} while(--remaining!=0U);
		return false;
	}

	template<bool HasTx,bool HasRx>
	static bool data(Bus& bus,const uint8_t* tx,uint8_t* rx,
		uint8_t count){
		for(uint8_t i=0;i<count;++i){
			const uint8_t outgoing=HasTx?tx[i]:SPI_BLOCK_READ_FILL_BYTE;
			uint8_t incoming=0;
			if(!shift(bus,outgoing,incoming)){
				return false;
			}
			if(HasRx){
				rx[i]=incoming;
			}
		}
		return true;
	}
};

#endif
