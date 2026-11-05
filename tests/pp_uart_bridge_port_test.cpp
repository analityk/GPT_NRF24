#include "../GPT_NRF2401/pp_uart_bridge_port.h"
#include "../GPT_NRF2401/uart_reliable_stream.h"
#include "../GPT_NRF2401/byte_ring.h"
#include <cassert>
#include <cstring>

#define PP_BRIDGE_TEST_RING_SIZE 128U
#define PP_BRIDGE_TEST_STREAM_BYTES 1024U
#define PP_BRIDGE_TEST_FEED_BYTES 2U
#define PP_BRIDGE_TEST_DRAIN_BYTES 2U
#define PP_BRIDGE_TEST_MAX_TURNS 6000U
#define PP_BRIDGE_TEST_FORWARD_LOSS_PERIOD 23U
#define PP_BRIDGE_TEST_ACK_LOSS_PERIOD 17U
#define PP_BRIDGE_TEST_A_MULTIPLIER 37U
#define PP_BRIDGE_TEST_B_MULTIPLIER 53U
#define PP_BRIDGE_TEST_B_SALT 0xA5U
#define PP_BRIDGE_TEST_SESSION_ID 0x7171U
#define PP_BRIDGE_TEST_FIRST_BYTE 0x00U
#define PP_BRIDGE_TEST_SECOND_BYTE 0xFFU

struct TestUart {
	ByteRing<PP_BRIDGE_TEST_RING_SIZE> rx,tx;

	bool read(uint8_t& value){
		return rx.pop(value);
	}

	uint8_t available() const{
		return rx.available();
	}

	uint8_t freeSpace() const{
		return tx.freeSpace();
	}

	bool writeAll(const uint8_t* bytes,uint8_t count){
		if(count!=0 && !bytes){
			return false;
		}
		if(tx.freeSpace()<count){
			return false;
		}
		for(uint8_t i=0;i<count;++i){
			assert(tx.push(bytes[i]));
		}
		return true;
	}
};

struct TestPeer {
	TestUart uart;
	PpUartBridgePort<TestUart> port;
	PairedLink link;
	UartReliableStream<PpUartBridgePort<TestUart> > stream;
	uint8_t frame[PairedFrame::MAX_FRAME];
	uint8_t width;

	explicit TestPeer(bool leader):
		port(uart),link(leader,PP_BRIDGE_TEST_SESSION_ID),
		stream(port),width(0){
		stream.beginSession(PP_BRIDGE_TEST_SESSION_ID);
	}

	void prepare(){
		assert(stream.make(link,frame,width));
	}

	void receive(const uint8_t* data,uint8_t size){
		PairedFrame::View view;
		assert(PairedFrame::decode(data,size,view)==
			PairedFrame::Error::OK);
		if(link.observeStream(view)!=PairedLink::Receipt::WRONG_SESSION){
			(void)stream.accept(view);
		}
	}
};

static void testAtomicBridge(){
	TestUart serial;
	PpUartBridgePort<TestUart> bridge(serial);
	const uint8_t pattern[]={PP_BRIDGE_TEST_FIRST_BYTE,
		static_cast<uint8_t>('s'),static_cast<uint8_t>('w'),
		PP_BRIDGE_TEST_SECOND_BYTE};
	for(uint8_t i=0;i<sizeof(pattern);++i){
		assert(serial.rx.push(pattern[i]));
	}
	assert(bridge.available()==sizeof(pattern));
	for(uint8_t i=0;i<sizeof(pattern);++i){
		uint8_t b=0;
		assert(bridge.read(b));
		assert(b==pattern[i]);
	}
	assert(bridge.available()==0);
	assert(bridge.writeAll(pattern,sizeof(pattern)));
	for(uint8_t i=0;i<sizeof(pattern);++i){
		uint8_t b=0;
		assert(serial.tx.pop(b));
		assert(b==pattern[i]);
	}

	const uint8_t filler=0x3CU;
	while(serial.tx.freeSpace()>sizeof(pattern)-1U){
		assert(serial.tx.push(filler));
	}
	const uint8_t before=serial.tx.available();
	assert(!bridge.writeAll(pattern,sizeof(pattern)));
	assert(serial.tx.available()==before);
	assert(!bridge.writeAll(nullptr,sizeof(pattern)));
}

static void testFullDuplexLosses(){
	TestPeer a(true),b(false);
	uint8_t fromA[PP_BRIDGE_TEST_STREAM_BYTES];
	uint8_t fromB[PP_BRIDGE_TEST_STREAM_BYTES];
	uint8_t intoA[PP_BRIDGE_TEST_STREAM_BYTES];
	uint8_t intoB[PP_BRIDGE_TEST_STREAM_BYTES];
	for(unsigned i=0;i<PP_BRIDGE_TEST_STREAM_BYTES;++i){
		fromA[i]=static_cast<uint8_t>(i*PP_BRIDGE_TEST_A_MULTIPLIER);
		fromB[i]=static_cast<uint8_t>(
			(i*PP_BRIDGE_TEST_B_MULTIPLIER)^PP_BRIDGE_TEST_B_SALT);
	}
	fromA[0]=PP_BRIDGE_TEST_FIRST_BYTE;
	fromA[1]=static_cast<uint8_t>('s');
	fromA[2]=static_cast<uint8_t>('w');
	fromA[3]=PP_BRIDGE_TEST_SECOND_BYTE;
	fromB[0]=PP_BRIDGE_TEST_SECOND_BYTE;
	fromB[1]=static_cast<uint8_t>('w');
	fromB[2]=static_cast<uint8_t>('s');
	fromB[3]=PP_BRIDGE_TEST_FIRST_BYTE;

	unsigned fedA=0,fedB=0,gotA=0,gotB=0;
	b.prepare();
	uint8_t reply[PairedFrame::MAX_FRAME];
	uint8_t replyWidth=b.width;
	std::memcpy(reply,b.frame,replyWidth);

	for(unsigned turn=1;turn<PP_BRIDGE_TEST_MAX_TURNS;++turn){
		for(unsigned n=0;n<PP_BRIDGE_TEST_FEED_BYTES;++n){
			if(fedA<PP_BRIDGE_TEST_STREAM_BYTES &&
				a.uart.rx.freeSpace()!=0){
				assert(a.uart.rx.push(fromA[fedA++]));
			}
			if(fedB<PP_BRIDGE_TEST_STREAM_BYTES &&
				b.uart.rx.freeSpace()!=0){
				assert(b.uart.rx.push(fromB[fedB++]));
			}
		}
		a.prepare();
		if(turn%PP_BRIDGE_TEST_FORWARD_LOSS_PERIOD!=0){
			b.receive(a.frame,a.width);
			if(turn%PP_BRIDGE_TEST_ACK_LOSS_PERIOD!=0){
				a.receive(reply,replyWidth);
			}
			b.prepare();
			replyWidth=b.width;
			std::memcpy(reply,b.frame,replyWidth);
		}
		for(unsigned n=0;n<PP_BRIDGE_TEST_DRAIN_BYTES;++n){
			uint8_t value=0;
			if(a.uart.tx.pop(value)){
				assert(gotA<PP_BRIDGE_TEST_STREAM_BYTES);
				intoA[gotA++]=value;
			}
			if(b.uart.tx.pop(value)){
				assert(gotB<PP_BRIDGE_TEST_STREAM_BYTES);
				intoB[gotB++]=value;
			}
		}
		if(gotA==PP_BRIDGE_TEST_STREAM_BYTES &&
			gotB==PP_BRIDGE_TEST_STREAM_BYTES &&
			!a.stream.hasPending() && !b.stream.hasPending()){
			break;
		}
	}
	assert(fedA==PP_BRIDGE_TEST_STREAM_BYTES);
	assert(fedB==PP_BRIDGE_TEST_STREAM_BYTES);
	assert(gotA==PP_BRIDGE_TEST_STREAM_BYTES);
	assert(gotB==PP_BRIDGE_TEST_STREAM_BYTES);
	assert(std::memcmp(intoA,fromB,PP_BRIDGE_TEST_STREAM_BYTES)==0);
	assert(std::memcmp(intoB,fromA,PP_BRIDGE_TEST_STREAM_BYTES)==0);
	assert(!a.stream.hasPending());
	assert(!b.stream.hasPending());
	assert(a.stream.retransmits()!=0 || b.stream.retransmits()!=0);
}

int main(){
	testAtomicBridge();
	testFullDuplexLosses();
}
