#include "../GPT_NRF2401/frame_v1.h"
#include <cassert>
#include <cstdint>
#include <cstring>
using namespace FrameV1;

static void roundTrip(const uint8_t (&wire)[32]) {
    View view;
    assert(decode(wire,view)==Error::OK);
    uint8_t encoded[32]={};
    assert(encode(view.header,view.body,encoded)==Error::OK);
    assert(std::memcmp(wire,encoded,32)==0);
}
static void expectError(const uint8_t (&wire)[32],Error error) {
    View view;
    view.header.src=23; // no mutation on malformed frame
    assert(decode(wire,view)==error);
    if (error!=Error::UNKNOWN_CONTROL)
        assert(view.header.src==23 && view.body==0);
}
int main() {
    const uint8_t hello[32]={
        0x67,0x00,0xFF,0x00,0x00,0xFF,0x00,0x00,
        0x01,0x78,0x56,0x34,0x12,0x01,0x00
    };
    const uint8_t unrecognisedData[32]={
        0x47,0x00,0x01,0x2A,0x10,0x05,0x02,0x02,
        0x48,0x65,0x6C,0x6C,0x6F,0x21,0x0D
    };
    const uint8_t heartbeat[32]={
        0x40,0x01,0x00,0x2A,0x06,0x10,0x00,0x00
    };
    const uint8_t reliable[32]={
        0x46,0x00,0x01,0x2A,0x11,0x06,0x01,0x01,
        0x00,0x41,0x42,0x43,0x44,0x45
    };
    const uint8_t ack[32]={
        0x65,0x01,0x00,0x2A,0x07,0x11,0x00,0x00,
        0x10,0x00,0x00,0x78,0x00
    };
    roundTrip(hello);
    roundTrip(unrecognisedData);
    roundTrip(heartbeat);
    roundTrip(reliable);
    roundTrip(ack);
    const uint8_t combined[32]={
        0x67,0x01,0x00,0x2A,0x08,0x11,0x00,0x00,
        0x12,0x00,0x00,0x78,0x00,0x01,0x41
    };
    roundTrip(combined);
    // LINK_REPORT: 10 B total, including the one-byte control opcode.
    const uint8_t linkReport[32]={
        0x6A,0x00,0x01,0x2A,0x03,0x02,0x00,0x00,
        0x11,0x00,0x01,0x00,0x00,0x00,0x03,0x00,0x00,0x00
    };
    roundTrip(linkReport);

    uint8_t sample[32]={};
    View view;
    std::memcpy(sample,hello,32);
    sample[0]=0xA7;
    expectError(sample,Error::BAD_VERSION);
    sample[0]=0x67;
    sample[16]=0x5A; // non-zero zero padding
    expectError(sample,Error::BAD_PADDING);
    std::memcpy(sample,reliable,32);
    sample[0]=0x59; // DATA LEN=25
    expectError(sample,Error::BAD_LENGTH);
    std::memcpy(sample,reliable,32);
    sample[2]=0xFF; // DATA broadcast not supported in v1
    expectError(sample,Error::BAD_ADDRESS);
    sample[2]=0x01; sample[3]=0;
    expectError(sample,Error::BAD_SESSION);
    sample[3]=0x2A; sample[6]=0;
    expectError(sample,Error::BAD_ENDPOINT);
    std::memcpy(sample,hello,32);
    sample[2]=0x01; // unicast first: broadcast READY would fail address validation
    sample[8]=0x03; // READY while SID=0 (HELLO-only)
    expectError(sample,Error::BAD_SESSION);
    std::memcpy(sample,linkReport,32);
    sample[0]=0x69; // 9 B would omit one byte of MAX_RT16
    expectError(sample,Error::BAD_CONTROL_LENGTH);
    std::memcpy(sample,combined,32);
    sample[0]=0x66; // combined DATA_ACK requires at least 7 body bytes
    sample[14]=0; // clear padding beyond LEN=6
    expectError(sample,Error::BAD_CONTROL_LENGTH);
    std::memcpy(sample,combined,32);
    sample[10]=0xFE; // unknown ACK status must not be treated as success
    assert(decode(sample,view)==Error::UNKNOWN_CONTROL);
    std::memcpy(sample,ack,32);
    sample[6]=1;
    expectError(sample,Error::BAD_ENDPOINT);
    std::memcpy(sample,ack,32);
    sample[0]=0x64; // ACK wrong length
    sample[12]=0;
    expectError(sample,Error::BAD_CONTROL_LENGTH);
    std::memcpy(sample,ack,32);
    sample[10]=0x7E; // unknown ACK status; never ACCEPTED
    assert(decode(sample,view)==Error::UNKNOWN_CONTROL);
    std::memcpy(sample,ack,32);
    sample[8]=0x77; // unknown CONTROL opcode
    assert(decode(sample,view)==Error::UNKNOWN_CONTROL);
    std::memcpy(sample,ack,32);
    sample[0]=0x63; sample[8]=0x7F; // EXT len 3; padding 11..31 zero
    sample[9]=0x43; sample[10]=0x25; sample[11]=0; sample[12]=0;
    assert(decode(sample,view)==Error::UNKNOWN_CONTROL);
    std::memcpy(sample,reliable,32);
    sample[0]=0x58; // full 24-byte BODY
    for (uint8_t i=0; i<24; ++i) sample[8+i]=static_cast<uint8_t>(i*11);
    assert(decode(sample,view)==Error::OK && view.header.length==24);
    roundTrip(sample);
    FrameV1::Header h=view.header;
    uint8_t out[32]={};
    h.length=25;
    assert(encode(h,view.body,out)==Error::BAD_LENGTH);
    assert(encode(h,view.body,0)==Error::NULL_BUFFER);
    assert(decode(0,view)==Error::NULL_BUFFER);
}
