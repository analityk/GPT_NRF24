#ifndef GPT_NRF24_PAIRED_FRAME_H
#define GPT_NRF24_PAIRED_FRAME_H
#include <stdint.h>
#include "retained_byte_ring.h"

// Paired two-node profile. Binary header offsets are explicit: no packed ABI.
// This is a NEW negotiated profile; it must not be decoded as legacy FrameV1.
namespace PairedFrame {
static const uint8_t HEADER_SIZE = 8;
static const uint8_t MAX_BODY = 24;
static const uint8_t MAX_FRAME = 32;
enum class Kind : uint8_t { DATA = 0, IDLE = 1, CONTROL = 2, EXT = 3 };
enum class Error : uint8_t { OK, NULL_BUFFER, BAD_LENGTH, BAD_TYPE };

enum Flag : uint8_t {
	// On DATA/IDLE: receiver requests fast Go-back-N replay starting
	// at RX_LAST+1. No extra packet bytes or transport payload header.
	URGENT = 0x04,
	PAUSE = 0x08,
	RESYNC = 0x10,
	WANT_PTX = 0x20,
	OVERFLOW = 0x40,
	REPORT = 0x80
};

struct Header {
	uint8_t txSeq, rxLast, lossAB, lossBA, arcLast, rxDrop, rxCredit, flags;
	uint16_t sessionId;
	Header();
	Kind kind() const;
};

struct View {
	Header header;
	const uint8_t* body;
	uint8_t length;
	View();
};

Error validate(const Header& h, uint8_t length);
// Header offsets are the on-air protocol layout, not an AVR ABI.
#define PAIRED_FRAME_TX_SEQ_OFFSET 0U
#define PAIRED_FRAME_RX_LAST_OFFSET 1U
#define PAIRED_FRAME_LOSS_OFFSET 2U
#define PAIRED_FRAME_DROP_RETRY_OFFSET 3U
#define PAIRED_FRAME_CREDIT_OFFSET 4U
#define PAIRED_FRAME_FLAGS_OFFSET 5U
#define PAIRED_FRAME_SID_LOW_OFFSET 6U
#define PAIRED_FRAME_SID_HIGH_OFFSET 7U
#define PAIRED_FRAME_NIBBLE_SHIFT 4U
#define PAIRED_FRAME_BYTE_BITS 8U
#define PAIRED_FRAME_KIND_MASK 0x03U
#define PAIRED_FRAME_NIBBLE_MASK 0x0FU
#define PAIRED_FRAME_NIBBLE_OVERFLOW_MASK 0xF0U
#define PAIRED_FRAME_INITIAL_ACK UINT8_MAX

void encodeHeader(const Header& h, uint8_t* dst);

Error encode(const Header& h, const uint8_t* body, uint8_t length, uint8_t* dst, uint8_t& width);

// Serialize from a retained ring straight into the one radio frame.
// The source may wrap at 255 -> 0; no staging BODY array is required.
Error encodeFrom(const Header& h, const RetainedByteRing& source, uint8_t first, uint8_t length,
	uint8_t* dst, uint8_t& width);

// RF dynamic width is the ONLY length source; do not require zero padding.
Error decode(const uint8_t* src, uint8_t width, View& dst);
} // namespace PairedFrame
#endif
