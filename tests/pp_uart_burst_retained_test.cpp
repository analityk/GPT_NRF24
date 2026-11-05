#include "../GPT_NRF2401/pp_uart_bridge_port.h"
#include "../GPT_NRF2401/uart_reliable_stream.h"
#include "../GPT_NRF2401/byte_ring.h"
#include <cassert>
#include <cstdint>
#include <cstring>

#define PP_BURST_RING_BYTES RETAINED_RING_MAX_BYTES
#define PP_BURST_COUNT 2U
#define PP_BURST_TOTAL_BYTES (PP_BURST_COUNT*PP_BURST_RING_BYTES)
#define PP_BURST_UART_TX_SIZE 128U
#define PP_BURST_DRAIN_PER_TURN 19U
#define PP_BURST_MAX_TURNS 900U
#define PP_BURST_SESSION 0xBEEF
#define PP_BURST_A_MUL 37U
#define PP_BURST_B_MUL 53U
#define PP_BURST_A_SALT 0xA5U
#define PP_BURST_B_SALT 0x5AU
#define PP_BURST_FIRST 0x00U
#define PP_BURST_LAST 0xFFU

struct BurstUart {
	RetainedByteRing rx;
	ByteRing<PP_BURST_UART_TX_SIZE> tx;

	RetainedByteRing& retainedInput(){
		return rx;
	}

	uint8_t available() const{
		return rx.available();
	}

	uint8_t freeSpace() const{
		return tx.freeSpace();
	}

	bool writeAll(const uint8_t* bytes,uint8_t count){
		if(tx.freeSpace()<count){
			return false;
		}
		for(uint8_t i=0;i<count;++i){
			assert(tx.push(bytes[i]));
		}
		return true;
	}
};

struct BurstPeer {
	BurstUart uart;
	PpUartBridgePort<BurstUart> port;
	PairedLink link;
	UartReliableStream<PpUartBridgePort<BurstUart>,true> stream;
	uint8_t packet[PairedFrame::MAX_FRAME];
	uint8_t width;

	explicit BurstPeer(bool leader):
		port(uart),link(leader,PP_BURST_SESSION),
		stream(port),width(0){
		stream.beginSession(PP_BURST_SESSION);
	}

	void make(){
		assert(stream.make(link,packet,width));
	}

	void accept(const uint8_t* bytes,uint8_t count){
		PairedFrame::View frame;
		assert(PairedFrame::decode(bytes,count,frame)==
			PairedFrame::Error::OK);
		if(link.observeStream(frame)!=PairedLink::Receipt::WRONG_SESSION){
			(void)stream.accept(frame);
		}
	}
};

static void drain(BurstPeer& peer,uint8_t* captured,unsigned& count){
	uint8_t byte=0;
	for(uint8_t n=0;n<PP_BURST_DRAIN_PER_TURN && peer.uart.tx.pop(byte);++n){
		assert(count<PP_BURST_TOTAL_BYTES);
		captured[count++]=byte;
	}
}

int main(){
	BurstPeer a(true),b(false);
	uint8_t sentA[PP_BURST_TOTAL_BYTES];
	uint8_t sentB[PP_BURST_TOTAL_BYTES];
	uint8_t seenA[PP_BURST_TOTAL_BYTES];
	uint8_t seenB[PP_BURST_TOTAL_BYTES];
	unsigned receivedA=0,receivedB=0;
	for(unsigned i=0;i<PP_BURST_TOTAL_BYTES;++i){
		sentA[i]=static_cast<uint8_t>((i*PP_BURST_A_MUL)^PP_BURST_A_SALT);
		sentB[i]=static_cast<uint8_t>((i*PP_BURST_B_MUL)^PP_BURST_B_SALT);
	}
	sentA[0]=PP_BURST_FIRST;
	sentB[0]=PP_BURST_LAST;
	sentA[PP_BURST_RING_BYTES-1U]=PP_BURST_LAST;
	sentB[PP_BURST_RING_BYTES-1U]=PP_BURST_FIRST;

	// Hardware ACK Payload from B is queued before receiving A's DATA.
	b.make();
	uint8_t queuedB[PairedFrame::MAX_FRAME];
	uint8_t queuedWidth=b.width;
	std::memcpy(queuedB,b.packet,queuedWidth);
	a.make();
	b.accept(a.packet,a.width);
	a.accept(queuedB,queuedWidth);
	b.make();
	queuedWidth=b.width;
	std::memcpy(queuedB,b.packet,queuedWidth);

	for(unsigned burst=0;burst<PP_BURST_COUNT;++burst){
		const unsigned begin=burst*PP_BURST_RING_BYTES;
		const unsigned finish=begin+PP_BURST_RING_BYTES;
		for(unsigned i=begin;i<finish;++i){
			assert(a.uart.rx.push(sentA[i]));
			assert(b.uart.rx.push(sentB[i]));
		}
		assert(a.port.pauseHostInput());
		assert(b.port.pauseHostInput());
		assert(a.uart.rx.available()==PP_BURST_RING_BYTES);
		assert(b.uart.rx.available()==PP_BURST_RING_BYTES);
		for(unsigned turn=0;turn<PP_BURST_MAX_TURNS;++turn){
			a.make();
			b.accept(a.packet,a.width);
			a.accept(queuedB,queuedWidth);
			b.make();
			queuedWidth=b.width;
			std::memcpy(queuedB,b.packet,queuedWidth);
			drain(a,seenA,receivedA);
			drain(b,seenB,receivedB);
			if(receivedA==finish && receivedB==finish &&
				!a.stream.hasPending() && !b.stream.hasPending()){
				break;
			}
		}
		assert(receivedA==finish);
		assert(receivedB==finish);
		assert(!a.stream.hasPending() && !b.stream.hasPending());
		assert(!a.port.pauseHostInput());
		assert(!b.port.pauseHostInput());
		assert(a.uart.rx.available()==0);
		assert(b.uart.rx.available()==0);
		assert(a.stream.inputFreeSpace()==RETAINED_RING_MAX_BYTES);
		assert(b.stream.inputFreeSpace()==RETAINED_RING_MAX_BYTES);
	}
	assert(std::memcmp(seenB,sentA,PP_BURST_TOTAL_BYTES)==0);
	assert(std::memcmp(seenA,sentB,PP_BURST_TOTAL_BYTES)==0);
	assert(a.stream.acknowledgedBytes()==PP_BURST_TOTAL_BYTES);
	assert(b.stream.acknowledgedBytes()==PP_BURST_TOTAL_BYTES);

	// No input must still produce consecutive IDLE packets (and ACK).
	for(unsigned turn=0;turn<UART_RELIABLE_TX_WINDOW_FRAMES+2U;++turn){
		a.make();
		assert(a.width==PairedFrame::HEADER_SIZE);
		b.accept(a.packet,a.width);
		a.accept(queuedB,queuedWidth);
		b.make();
		queuedWidth=b.width;
		std::memcpy(queuedB,b.packet,queuedWidth);
		assert(b.width==PairedFrame::HEADER_SIZE);
	}
}
