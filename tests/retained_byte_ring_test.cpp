#include "../GPT_NRF2401/retained_byte_ring.h"
#include "../GPT_NRF2401/paired_link.h"
#include <cassert>
#include <cstdint>
#include <cstring>

#define RING_TEST_FIRST_BYTE 0xA5U
#define RING_TEST_OTHER_BYTE 0x5AU
#define RING_TEST_ACK_BYTES 96U
#define RING_TEST_REPLAY_START 251U
#define RING_TEST_REPLAY_COUNT 12U
#define RING_TEST_SID 0xBEEFU
#define RING_TEST_SEQ 17U

static void testIndicesAndHysteresis(){
	RetainedByteRing ring;
	assert(ring.available()==0);
	assert(ring.freeSpace()==RETAINED_RING_MAX_BYTES);
	assert(!ring.pauseRequested());
	assert(ring.availableToPrepare()==0);
	uint8_t b=0;
	assert(!ring.pop(b));

	for(uint16_t i=0;i<RETAINED_RING_MAX_BYTES;++i){
		assert(ring.push(static_cast<uint8_t>(i)));
	}
	assert(ring.available()==RETAINED_RING_MAX_BYTES);
	assert(ring.freeSpace()==0);
	assert(ring.pauseRequested());
	assert(!ring.push(RING_TEST_FIRST_BYTE));
	assert(ring.availableToPrepare()==RETAINED_RING_MAX_BYTES);
	const uint8_t start=ring.prepared();
	ring.markPrepared(RING_TEST_ACK_BYTES);
	assert(ring.prepared()==static_cast<uint8_t>(start+RING_TEST_ACK_BYTES));
	ring.acknowledge(RING_TEST_ACK_BYTES);
	assert(ring.available()==RETAINED_RING_MAX_BYTES-RING_TEST_ACK_BYTES);
	assert(!ring.pauseRequested());
	// Unreleased DATA bytes are never overwritten or erased.
	assert(ring.at(static_cast<uint8_t>(RING_TEST_ACK_BYTES))==
		RING_TEST_ACK_BYTES);
	for(uint8_t i=0;i<RING_TEST_ACK_BYTES;++i){
		assert(ring.push(static_cast<uint8_t>(i^RING_TEST_OTHER_BYTE)));
	}
	assert(ring.available()==RETAINED_RING_MAX_BYTES);
	assert(ring.pauseRequested());
	assert(ring.at(static_cast<uint8_t>(RING_TEST_ACK_BYTES))==
		RING_TEST_ACK_BYTES);
}

static void testWrapAndEncoding(){
	RetainedByteRing ring;
	// Prepare/ACK most of the ring so the next packet crosses index 255.
	for(uint16_t i=0;i<RETAINED_RING_MAX_BYTES;++i){
		assert(ring.push(static_cast<uint8_t>(i)));
	}
	ring.markPrepared(RETAINED_RING_MAX_BYTES);
	ring.acknowledge(RETAINED_RING_MAX_BYTES);
	assert(ring.available()==0);
	const uint8_t first=ring.prepared();
	assert(first==RETAINED_RING_MAX_BYTES);
	for(uint8_t i=0;i<RING_TEST_REPLAY_COUNT;++i){
		assert(ring.push(static_cast<uint8_t>(i^RING_TEST_OTHER_BYTE)));
	}
	PairedLink link(true,RING_TEST_SID);
	uint8_t frame[PairedFrame::MAX_FRAME];
	uint8_t width=0;
	assert(link.makeStreamFrom(RING_TEST_SEQ,0U,1U,ring,first,
		RING_TEST_REPLAY_COUNT,frame,width)==PairedFrame::Error::OK);
	assert(width==PairedFrame::HEADER_SIZE+RING_TEST_REPLAY_COUNT);
	PairedFrame::View decoded;
	assert(PairedFrame::decode(frame,width,decoded)==PairedFrame::Error::OK);
	for(uint8_t i=0;i<RING_TEST_REPLAY_COUNT;++i){
		assert(decoded.body[i]==static_cast<uint8_t>(i^RING_TEST_OTHER_BYTE));
	}
	// Replays read exactly the same ring positions.
	uint8_t repeated[PairedFrame::MAX_FRAME];
	uint8_t repeatWidth=0;
	assert(link.makeStreamFrom(RING_TEST_SEQ,0U,1U,ring,first,
		RING_TEST_REPLAY_COUNT,repeated,repeatWidth)==PairedFrame::Error::OK);
	assert(repeatWidth==width);
	assert(std::memcmp(frame,repeated,width)==0);
}

int main(){
	testIndicesAndHysteresis();
	testWrapAndEncoding();
}
