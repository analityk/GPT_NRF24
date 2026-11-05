#include "../GPT_NRF2401/pp_reply_mailbox.h"
#include "../GPT_NRF2401/uart_reliable_stream.h"
#include "../GPT_NRF2401/byte_ring.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

#define PP_DEFER_TEST_SESSION_ID 0x4721U
#define PP_DEFER_TEST_BYTES 2048U
#define PP_DEFER_TEST_MAX_EXCHANGES 30000U
#define PP_DEFER_TEST_UART_RING_BYTES 128U
#define PP_DEFER_TEST_FEED_BYTES 3U
#define PP_DEFER_TEST_DRAIN_BYTES 2U
#define PP_DEFER_TEST_MISSING_DATA_PERIOD 17U
#define PP_DEFER_TEST_SECOND_MISSING_DATA_PERIOD 43U
#define PP_DEFER_TEST_MISSING_ACK_PERIOD 13U
#define PP_DEFER_TEST_SOURCE_FACTOR_A 37U
#define PP_DEFER_TEST_SOURCE_FACTOR_B 11U
#define PP_DEFER_TEST_BYTE_MASK 0xFFU

struct DeferredPort {
	ByteRing<PP_DEFER_TEST_UART_RING_BYTES> input;
	ByteRing<PP_DEFER_TEST_UART_RING_BYTES> output;

	bool read(uint8_t& byte){
		return input.pop(byte);
	}

	uint8_t available() const{
		return input.available();
	}

	uint8_t freeSpace() const{
		return output.freeSpace();
	}

	bool writeAll(const uint8_t* data,uint8_t count){
		if(output.freeSpace()<count){
			return false;
		}
		for(uint8_t i=0;i<count;++i){
			assert(output.push(data[i]));
		}
		return true;
	}
};

struct DeferredPeer {
	DeferredPort serial;
	PairedLink link;
	UartReliableStream<DeferredPort> stream;
	uint8_t frame[PairedFrame::MAX_FRAME];
	uint8_t width;

	explicit DeferredPeer(bool leader):
		link(leader,PP_DEFER_TEST_SESSION_ID),stream(serial),width(0){
		stream.beginSession(PP_DEFER_TEST_SESSION_ID);
	}

	void prepare(){
		assert(stream.make(link,frame,width));
	}

	void receive(const uint8_t* frameBytes,uint8_t frameWidth){
		PairedFrame::View view;
		assert(PairedFrame::decode(frameBytes,frameWidth,view)==
			PairedFrame::Error::OK);
		assert(link.observeStream(view)!=
			PairedLink::Receipt::WRONG_SESSION);
		(void)stream.accept(view);
	}
};

struct DeferredResult {
	uint8_t retries;
	DeferredResult():retries(0){}
};

int main(){
	DeferredPeer a(true),b(false);
	uint8_t ab[PP_DEFER_TEST_BYTES];
	uint8_t ba[PP_DEFER_TEST_BYTES];
	uint8_t outAB[PP_DEFER_TEST_BYTES];
	uint8_t outBA[PP_DEFER_TEST_BYTES];
	for(uint16_t i=0;i<PP_DEFER_TEST_BYTES;++i){
		ab[i]=static_cast<uint8_t>(i*PP_DEFER_TEST_SOURCE_FACTOR_A);
		ba[i]=static_cast<uint8_t>(i*PP_DEFER_TEST_SOURCE_FACTOR_B);
	}
	uint16_t fedA=0;
	uint16_t fedB=0;
	uint16_t receivedA=0;
	uint16_t receivedB=0;
	uint8_t ackQueue[PairedFrame::MAX_FRAME]={};
	uint8_t ackQueueWidth=0;
	uint8_t pendingReply[PairedFrame::MAX_FRAME]={};
	PpReplyMailbox<DeferredResult> deferred;
	b.prepare();
	ackQueueWidth=b.width;
	memcpy(ackQueue,b.frame,ackQueueWidth);

	for(uint16_t turn=0;turn<PP_DEFER_TEST_MAX_EXCHANGES;++turn){
		for(uint8_t i=0;i<PP_DEFER_TEST_FEED_BYTES &&
			fedA<PP_DEFER_TEST_BYTES && a.serial.input.freeSpace();++i){
			assert(a.serial.input.push(ab[fedA++]));
		}
		for(uint8_t i=0;i<PP_DEFER_TEST_FEED_BYTES &&
			fedB<PP_DEFER_TEST_BYTES && b.serial.input.freeSpace();++i){
			assert(b.serial.input.push(ba[fedB++]));
		}

		// A builds and launches the next TX with last turn's RX pending.
		a.prepare();
		if(deferred.pending()){
			DeferredResult previous;
			uint8_t previousWidth=0;
			assert(deferred.take(previous,previousWidth));
			assert(PpReplyMailbox<DeferredResult>::canDefer(
				true,PP_DEFER_TEST_SESSION_ID,pendingReply,previousWidth));
			a.receive(pendingReply,previousWidth);
		}

		const bool dataDelivered=(turn%PP_DEFER_TEST_MISSING_DATA_PERIOD)!=0 &&
			(turn%PP_DEFER_TEST_SECOND_MISSING_DATA_PERIOD)!=0;
		const bool ackDelivered=dataDelivered &&
			(turn%PP_DEFER_TEST_MISSING_ACK_PERIOD)!=0;
		if(dataDelivered){
			b.receive(a.frame,a.width);
			if(ackDelivered){
				memcpy(pendingReply,ackQueue,ackQueueWidth);
				DeferredResult outcome;
				assert(deferred.store(outcome,ackQueueWidth));
			}
			b.prepare();
			ackQueueWidth=b.width;
			memcpy(ackQueue,b.frame,ackQueueWidth);
		}

		for(uint8_t i=0;i<PP_DEFER_TEST_DRAIN_BYTES &&
			b.serial.output.available();++i){
			uint8_t value=0;
			assert(b.serial.output.pop(value));
			assert(receivedB<PP_DEFER_TEST_BYTES);
			outAB[receivedB++]=value;
		}
		for(uint8_t i=0;i<PP_DEFER_TEST_DRAIN_BYTES &&
			a.serial.output.available();++i){
			uint8_t value=0;
			assert(a.serial.output.pop(value));
			assert(receivedA<PP_DEFER_TEST_BYTES);
			outBA[receivedA++]=value;
		}
		if(receivedA==PP_DEFER_TEST_BYTES &&
			receivedB==PP_DEFER_TEST_BYTES &&
			!a.stream.hasPending() && !b.stream.hasPending()){
			break;
		}
	}
	assert(fedA==PP_DEFER_TEST_BYTES && fedB==PP_DEFER_TEST_BYTES);
	assert(receivedA==PP_DEFER_TEST_BYTES);
	assert(receivedB==PP_DEFER_TEST_BYTES);
	assert(memcmp(ab,outAB,PP_DEFER_TEST_BYTES)==0);
	assert(memcmp(ba,outBA,PP_DEFER_TEST_BYTES)==0);
	assert(!a.stream.hasPending() && !b.stream.hasPending());
	assert(a.stream.acknowledgedBytes()==PP_DEFER_TEST_BYTES);
	assert(b.stream.acknowledgedBytes()==PP_DEFER_TEST_BYTES);
	assert(a.stream.retransmits()>0 || b.stream.retransmits()>0);
}
