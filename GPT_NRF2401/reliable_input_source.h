#ifndef GPT_NRF24_RELIABLE_INPUT_SOURCE_H
#define GPT_NRF24_RELIABLE_INPUT_SOURCE_H

#include "retained_byte_ring.h"

// One transport implementation, two ways of owning its byte storage.
// Shared=true: ISR writes directly into the only retained UART RX ring.
// Shared=false: synthetic and host-test sources are copied into a local
// retained ring; no second per-frame retransmission cache is created.
template<class Port,bool Shared>
class ReliableInputSource;

template<class Port>
class ReliableInputSource<Port,false> {
public:
	explicit ReliableInputSource(Port& port):port_(port){
	}

	void pump(){
		uint8_t value=0;
		while(buffer_.freeSpace()!=0 && port_.read(value)){
			(void)buffer_.push(value);
		}
	}

	RetainedByteRing& ring(){
		return buffer_;
	}

	const RetainedByteRing& ring() const{
		return buffer_;
	}

	bool hasInput() const{
		return buffer_.availableToPrepare()!=0 || port_.available()!=0;
	}

private:
	Port& port_;
	RetainedByteRing buffer_;
};

template<class Port>
class ReliableInputSource<Port,true> {
public:
	explicit ReliableInputSource(Port& port):ring_(port.retainedInput()){
	}

	void pump(){
	}

	RetainedByteRing& ring(){
		return ring_;
	}

	const RetainedByteRing& ring() const{
		return ring_;
	}

	bool hasInput() const{
		return ring_.availableToPrepare()!=0;
	}

private:
	RetainedByteRing& ring_;
};

#endif
