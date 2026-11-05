#ifndef GPT_NRF24_RETAINED_BYTE_RING_H
#define GPT_NRF24_RETAINED_BYTE_RING_H

#include <stdint.h>

// A 256-byte SPSC ring for ACK-retained transmission. One byte is kept
// unused to distinguish full/empty. Every cursor fits one atomic AVR byte.
// ISR owns write_; foreground owns prepared_ and acknowledged_.
// Storage is never cleared when an ACK advances acknowledged_.
#define RETAINED_RING_CAPACITY 256U
#define RETAINED_RING_MASK 0xFFU
#define RETAINED_RING_MAX_BYTES 255U
#define RETAINED_RING_PAUSE_FREE_BYTES 48U
#define RETAINED_RING_RESUME_FREE_BYTES 96U

class RetainedByteRing {
public:
	RetainedByteRing():write_(0),prepared_(0),acknowledged_(0),pause_(false){
	}

	bool push(uint8_t byte){
		const uint8_t next=static_cast<uint8_t>(write_+1U);
		if(next==acknowledged_){
			pause_=true;
			return false;
		}
		data_[write_]=byte;
		write_=next;
		if(freeSpace()<=RETAINED_RING_PAUSE_FREE_BYTES){
			pause_=true;
		}
		return true;
	}

	// Ordinary consuming reader, for uses that do not retain UART data
	// (for example benchmark control commands).
	bool pop(uint8_t& byte){
		if(prepared_!=acknowledged_ || write_==acknowledged_){
			return false;
		}
		byte=data_[acknowledged_];
		prepared_=static_cast<uint8_t>(prepared_+1U);
		acknowledge(1U);
		return true;
	}

	uint8_t available() const{
		return static_cast<uint8_t>(write_-acknowledged_);
	}

	uint8_t availableToPrepare() const{
		return static_cast<uint8_t>(write_-prepared_);
	}

	uint8_t freeSpace() const{
		return static_cast<uint8_t>(RETAINED_RING_MAX_BYTES-available());
	}

	uint8_t prepared() const{
		return prepared_;
	}

	uint8_t at(uint8_t index) const{
		return data_[index];
	}

	// The caller has already serialized these bytes into a complete frame.
	void markPrepared(uint8_t count){
		prepared_=static_cast<uint8_t>(prepared_+count);
	}

	// Only a cumulative application ACK may release retained bytes.
	void acknowledge(uint8_t count){
		acknowledged_=static_cast<uint8_t>(acknowledged_+count);
		if(freeSpace()>=RETAINED_RING_RESUME_FREE_BYTES){
			pause_=false;
		}
	}

	bool pauseRequested() const{
		return pause_;
	}

	void reset(){
		write_=0;
		prepared_=0;
		acknowledged_=0;
		pause_=false;
	}

private:
	volatile uint8_t data_[RETAINED_RING_CAPACITY];
	volatile uint8_t write_;
	uint8_t prepared_;
	volatile uint8_t acknowledged_;
	volatile bool pause_;
};

#endif
