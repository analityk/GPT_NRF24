#ifndef GPT_NRF24_PP_REPLY_MAILBOX_H
#define GPT_NRF24_PP_REPLY_MAILBOX_H

#include <stdint.h>
#include "paired_frame.h"

// One outstanding RX buffer, owned by the caller. Metadata is deferred
// until the NEXT radio exchange is on air. The caller MUST consume the
// mailbox before writing any new SPI reply into that RX buffer.
// No extra frame storage or heap is needed.
template<class Outcome>
class PpReplyMailbox {
public:
	PpReplyMailbox():pending_(false),width_(0),outcome_(){
	}


	bool pending() const{
		return pending_;
	}

	bool store(const Outcome& outcome,uint8_t width){
		if(pending_){
			return false;
		}
		outcome_=outcome;
		width_=width;
		pending_=true;
		return true;
	}

	bool take(Outcome& outcome,uint8_t& width){
		if(!pending_){
			return false;
		}
		outcome=outcome_;
		width=width_;
		pending_=false;
		return true;
	}

	// A valid active DATA or IDLE cannot change the handshake epoch.
	// CONTROL, mismatched SID and malformed frames are processed before
	// launching the next TX: they may require a FIFO reset or new nonce.
	static bool canDefer(bool active,uint16_t sid,const uint8_t* frame,
		uint8_t width){
		if(!active){
			return false;
		}
		PairedFrame::View view;
		if(PairedFrame::decode(frame,width,view)!=PairedFrame::Error::OK){
			return false;
		}
		const PairedFrame::Kind kind=view.header.kind();
		return view.header.sessionId==sid &&
			(kind==PairedFrame::Kind::DATA ||
			 kind==PairedFrame::Kind::IDLE);
	}

private:
	bool pending_;
	uint8_t width_;
	Outcome outcome_;
};

#endif
