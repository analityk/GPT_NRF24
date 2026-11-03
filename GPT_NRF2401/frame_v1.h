#ifndef GPT_NRF24_FRAME_V1_H
#define GPT_NRF24_FRAME_V1_H

#include <stdint.h>

// Protocol v1 wire format, independent of ATmega328P, SPI and nRF24 hardware.
// Encode/decode a fixed 32-byte RF payload; no packed structs, heap or RTTI.
// The radio adds its own hardware CRC16 OUTSIDE these 32 bytes.
namespace FrameV1 {
    static const uint8_t FRAME_SIZE=32;
    static const uint8_t HEADER_SIZE=8;
    static const uint8_t BODY_SIZE=24;
    static const uint8_t BROADCAST=0xFF;
    static const uint8_t ENDPOINT_CONTROL=0;
    static const uint8_t ENDPOINT_UART_RELIABLE=1;

    enum class Kind : uint8_t { DATA=0, CONTROL=1 };
    enum class Error : uint8_t {
        OK=0, NULL_BUFFER, BAD_VERSION, BAD_LENGTH, BAD_PADDING,
        BAD_ADDRESS, BAD_ENDPOINT, BAD_SESSION, BAD_CONTROL_LENGTH,
        UNKNOWN_CONTROL
    };

    struct Header {
        Kind kind;
        uint8_t src, dst, sid, txSeq, rxLast, srcEp, dstEp, length;
        Header() : kind(Kind::DATA), src(0), dst(0), sid(0),
                   txSeq(0), rxLast(0xFF), srcEp(0), dstEp(0), length(0) {}
    };

    struct View {
        Header header;
        const uint8_t* body;
        View() : header(), body(0) {}
    };

    inline uint8_t expectedControlLength(uint8_t opcode) {
        switch (opcode) {
            case 0x01: return 7;  // HELLO
            case 0x02: return 11; // HELLO_REPLY
            case 0x03: return 10; // READY
            case 0x04: return 2;  // READY_ACK
            case 0x10: return 5;  // TRANSPORT_ACK
            case 0x11: return 10; // LINK_REPORT: opcode + 1 + 4 + 2 + 2 bytes
            case 0x7F: return 0;  // EXT: length >= 3
            default: return 0;    // unknown extension/control
        }
    }

    // Stateless structural validation. Unknown CONTROL opcodes and namespaces
    // can be distinguished without ever being dispatched as supported services.
    inline Error validate(const Header& h, const uint8_t* body) {
        if (h.length > BODY_SIZE) return Error::BAD_LENGTH;
        if (h.length != 0 && !body) return Error::NULL_BUFFER;
        if (h.src >= 0xFE || h.dst == 0xFE) return Error::BAD_ADDRESS;
        if (h.kind == Kind::DATA) {
            if (h.dst == BROADCAST) return Error::BAD_ADDRESS;
            if (h.sid == 0) return Error::BAD_SESSION;
            if (h.length == 0) {
                if (h.srcEp != 0 || h.dstEp != 0) return Error::BAD_ENDPOINT;
            } else if (h.srcEp == 0 || h.dstEp == 0) {
                return Error::BAD_ENDPOINT;
            }
            return Error::OK;
        }
        if (h.kind != Kind::CONTROL) return Error::BAD_VERSION;
        if (h.srcEp != 0 || h.dstEp != 0) return Error::BAD_ENDPOINT;
        if (h.length == 0) return Error::BAD_CONTROL_LENGTH;
        const uint8_t opcode=body[0];
        if (h.dst == BROADCAST && opcode != 0x01)
            return Error::BAD_ADDRESS;
        if (opcode == 0x01 || opcode == 0x02) {
            if (h.sid != 0 || h.txSeq != 0 || h.rxLast != 0xFF)
                return Error::BAD_SESSION;
        } else if (h.sid == 0) {
            return Error::BAD_SESSION;
        }
        if (opcode == 0x7F) {
            if (h.length < 3) return Error::BAD_CONTROL_LENGTH;
            return Error::UNKNOWN_CONTROL; // valid EXT envelope; no extension implemented
        }
        const uint8_t exact=expectedControlLength(opcode);
        if (exact == 0) return Error::UNKNOWN_CONTROL;
        if (h.length != exact) return Error::BAD_CONTROL_LENGTH;
        if (opcode == 0x10 && body[2] > 2)
            return Error::UNKNOWN_CONTROL; // unknown ACK status is not ACCEPTED
        return Error::OK;
    }

    // Emits all 32 bytes (zeroes tail); does not touch the output on failure.
    // UNKNOWN_CONTROL is only allowed on the decoder side, not originated.
    inline Error encode(const Header& h, const uint8_t* body, uint8_t* out) {
        if (!out) return Error::NULL_BUFFER;
        const Error result=validate(h,body);
        if (result != Error::OK) return result;
        out[0]=static_cast<uint8_t>(0x40U |
                 (h.kind==Kind::CONTROL ? 0x20U : 0U) | h.length);
        out[1]=h.src;
        out[2]=h.dst;
        out[3]=h.sid;
        out[4]=h.txSeq;
        out[5]=h.rxLast;
        out[6]=h.srcEp;
        out[7]=h.dstEp;
        for (uint8_t i=0; i<BODY_SIZE; ++i)
            out[HEADER_SIZE+i]=(i<h.length) ? body[i] : 0;
        return Error::OK;
    }

    // On malformed input, caller's View remains unchanged. Unknown control
    // opcodes return UNKNOWN_CONTROL with a complete View for safe skipping.
    inline Error decode(const uint8_t* raw, View& out) {
        if (!raw) return Error::NULL_BUFFER;
        if ((raw[0] & 0xC0U) != 0x40U) return Error::BAD_VERSION;
        Header h;
        h.kind=(raw[0] & 0x20U) ? Kind::CONTROL : Kind::DATA;
        h.length=static_cast<uint8_t>(raw[0] & 0x1FU);
        if (h.length>BODY_SIZE) return Error::BAD_LENGTH;
        for (uint8_t i=h.length; i<BODY_SIZE; ++i) {
            if (raw[HEADER_SIZE+i] != 0) return Error::BAD_PADDING;
        }
        h.src=raw[1]; h.dst=raw[2]; h.sid=raw[3]; h.txSeq=raw[4];
        h.rxLast=raw[5]; h.srcEp=raw[6]; h.dstEp=raw[7];
        const uint8_t* body=raw+HEADER_SIZE;
        const Error result=validate(h,body);
        if (result == Error::OK || result == Error::UNKNOWN_CONTROL) {
            out.header=h;
            out.body=body;
        }
        return result;
    }
}
#endif
