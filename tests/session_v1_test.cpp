#include "../GPT_NRF2401/session_v1.h"
#include <cassert>
#include <cstdint>

static FrameV1::View decode(const uint8_t (&raw)[32]) {
    FrameV1::View v;
    assert(FrameV1::decode(raw,v)==FrameV1::Error::OK);
    return v;
}
int main() {
    SessionV1 a(0,1,17), b(1,0,33);
    assert(a.isLeader() && !b.isLeader());
    assert(!a.active() && !b.active());
    uint8_t raw[32]={};

    // A HELLO -> B
    assert(a.prepare(raw));
    assert(raw[8]==0x01);
    assert(b.accept(decode(raw)));
    // B HELLO_REPLY -> A
    assert(b.prepare(raw));
    assert(raw[8]==0x02);
    assert(a.accept(decode(raw)));
    b.sentReply();
    // A READY -> B; B becomes active before READY_ACK TX.
    assert(a.prepare(raw));
    assert(raw[8]==0x03);
    uint8_t ready[32]={};
    for (unsigned i=0;i<32;++i) ready[i]=raw[i];
    assert(b.accept(decode(raw)));
    assert(b.active() && !a.active());
    assert(b.prepare(raw) && raw[8]==0x04);
    assert(a.accept(decode(raw)));
    b.sentReply();
    assert(a.active() && b.active() && a.sid()==b.sid());
    assert(a.generation()==1 && b.generation()==1);

    // Lost READY_ACK -> B must retransmit it on duplicate READY.
    assert(b.accept(decode(ready)));
    assert(b.prepare(raw) && raw[8]==0x04);
    b.sentReply();
    assert(b.generation()==1); // no second session / dedup reset

    // A's new discovery nonce when it restarts changes the RF session.
    const uint8_t oldSid=a.sid();
    a.restart();
    assert(!a.active());
    assert(a.prepare(raw) && raw[8]==0x01);
    assert(b.accept(decode(raw)));
    assert(!b.active());
    assert(b.prepare(raw) && raw[8]==0x02);
    assert(a.accept(decode(raw)));
    b.sentReply();
    assert(a.prepare(raw) && raw[8]==0x03);
    assert(b.accept(decode(raw)));
    assert(b.prepare(raw) && raw[8]==0x04);
    assert(a.accept(decode(raw)));
    b.sentReply();
    assert(a.active() && b.active());
    assert(a.sid()==b.sid());
    assert(a.sid()!=oldSid);
    assert(a.generation()==2 && b.generation()==2);

    // Reset B using a new persistent boot epoch.
    SessionV1 restartedB(1,0,34);
    a.restart();
    assert(a.prepare(raw));
    assert(restartedB.accept(decode(raw)));
    assert(restartedB.prepare(raw));
    assert(a.accept(decode(raw)));
    restartedB.sentReply();
    assert(a.prepare(raw));
    assert(restartedB.accept(decode(raw)));
    assert(restartedB.prepare(raw));
    assert(a.accept(decode(raw)));
    assert(a.active() && restartedB.active());
    assert(a.sid()==restartedB.sid());

    // Status check: never accept handshake with a wrong nonce.
    SessionV1 wrong(0,1,42);
    uint8_t forged[32]={};
    assert(a.prepare(forged)==false); // A active: no more HELLO
    wrong.configure(0,1,42);
    assert(wrong.prepare(forged));
    FrameV1::View v=decode(forged);
    assert(!a.accept(v)); // different role/state/source

    // Deterministic CRC-8 session derivation, not a secret token.
    assert(SessionV1::derive(17,33)==SessionV1::derive(17,33));
    assert(SessionV1::derive(17,33)!=0);
}
