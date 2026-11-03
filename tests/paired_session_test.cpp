#include "../GPT_NRF2401/paired_session.h"
#include <cassert>
#include <cstdint>
#include <cstring>

static PairedSession::Event deliver(PairedSession& tx,PairedSession& rx) {
    uint8_t buffer[32]={},width=0;
    assert(tx.prepare(buffer,width));
    PairedFrame::View v;
    assert(PairedFrame::decode(buffer,width,v)==PairedFrame::Error::OK);
    return rx.accept(v);
}
static void startPair(PairedSession& a,PairedSession& b) {
    assert(a.phase()==PairedSession::Phase::DISCOVER);
    assert(b.phase()==PairedSession::Phase::DISCOVER);
    // Simulate actual hardware ACK pipeline: B prepares BEACON before
    // receiving A's HELLO; the first radio reply cannot be WELCOME.
    uint8_t prepared[32]={},length=0;
    assert(b.prepare(prepared,length));
    PairedFrame::View earlier;
    assert(PairedFrame::decode(prepared,length,earlier)==PairedFrame::Error::OK);
    assert(deliver(a,b)==PairedSession::Event::CHANGED);
    assert(a.accept(earlier)==PairedSession::Event::NONE);
    assert(b.phase()==PairedSession::Phase::WAIT_READY);
    assert(deliver(b,a)==PairedSession::Event::CHANGED);
    assert(a.phase()==PairedSession::Phase::WAIT_READY_ACK);
    assert(deliver(a,b)==PairedSession::Event::CHANGED);
    assert(b.phase()==PairedSession::Phase::WAIT_CONFIRM);
    assert(deliver(b,a)==PairedSession::Event::ESTABLISHED);
    assert(a.active() && !b.active());
    PairedFrame::Header h;
    h.sessionId=a.sid();
    h.flags=static_cast<uint8_t>(PairedFrame::Kind::IDLE);
    PairedFrame::View data;
    uint8_t raw[32]={},width=0;
    assert(PairedFrame::encode(h,0,0,raw,width)==PairedFrame::Error::OK);
    assert(PairedFrame::decode(raw,width,data)==PairedFrame::Error::OK);
    assert(b.accept(data)==PairedSession::Event::ESTABLISHED);
    assert(b.active() && a.sid()==b.sid() && a.sid()!=0);
}
int main() {
    typedef PairedSession S;
    S a(true,S::Identity(10,0)),b(false,S::Identity(20,0));
    startPair(a,b);
    const uint16_t initialSid=a.sid();
    assert(S::derive(S::Identity(10,0),S::Identity(20,0))==initialSid);
    assert(S::derive(S::Identity(10,1),S::Identity(20,0))!=initialSid);

    // B reset: A ACTIVE receives reboot beacon and immediately invalidates
    // its previous session, then re-establishes with fresh identities.
    S freshB(false,S::Identity(21,0));
    assert(deliver(freshB,a)==S::Event::LOST);
    assert(!a.active() && a.own().attempt==1);
    assert(deliver(a,freshB)==S::Event::CHANGED);
    assert(deliver(freshB,a)==S::Event::CHANGED);
    assert(deliver(a,freshB)==S::Event::CHANGED);
    assert(deliver(freshB,a)==S::Event::ESTABLISHED);
    assert(a.active() && a.sid()!=initialSid);
    PairedFrame::Header h;
    h.sessionId=a.sid();
    h.flags=static_cast<uint8_t>(PairedFrame::Kind::DATA);
    uint8_t raw[32]={},width=0;
    assert(PairedFrame::encode(h,0,0,raw,width)==PairedFrame::Error::OK);
    PairedFrame::View v;
    assert(PairedFrame::decode(raw,width,v)==PairedFrame::Error::OK);
    assert(freshB.accept(v)==S::Event::ESTABLISHED);
    assert(freshB.active());
    // An old ACK beacon from the same B boot must NOT tear down ACTIVE.
    uint8_t oldBeacon[32]={};
    S tmpB(false,S::Identity(21,0));
    assert(tmpB.prepare(oldBeacon,width));
    assert(PairedFrame::decode(oldBeacon,width,v)==PairedFrame::Error::OK);
    assert(a.accept(v)==S::Event::NONE && a.active());

    // A reset: B accepts a different boot nonce and leaves ACTIVE.
    S newA(true,S::Identity(11,0));
    assert(deliver(newA,freshB)==S::Event::LOST);
    assert(!freshB.active());
    assert(deliver(freshB,newA)==S::Event::CHANGED);
    assert(deliver(newA,freshB)==S::Event::CHANGED);
    assert(deliver(freshB,newA)==S::Event::ESTABLISHED);
    assert(newA.active() && newA.sid()==freshB.sid());

    // A changed initiator nonce must update the responder even if its
    // state stays WAIT_READY: its previously queued WELCOME is then stale.
    S responder(false,S::Identity(71,0));
    S first(true,S::Identity(81,0));
    S second(true,S::Identity(82,0));
    assert(deliver(first,responder)==S::Event::CHANGED);
    assert(responder.phase()==S::Phase::WAIT_READY);
    assert(responder.peer()==S::Identity(81,0));
    assert(deliver(second,responder)==S::Event::CHANGED);
    assert(responder.phase()==S::Phase::WAIT_READY);
    assert(responder.peer()==S::Identity(82,0));

    // Malformed/foreign control never starts a session.
    S invalid(true,S::Identity(90,0));
    assert(invalid.prepare(raw,width));
    raw[8]=0xFF; // unknown handshake opcode, not a valid HELLO
    assert(PairedFrame::decode(raw,width,v)==PairedFrame::Error::OK);
    assert(freshB.accept(v)==S::Event::NONE);
    raw[8]=static_cast<uint8_t>(S::Op::HELLO);
    raw[6]=1; // CONTROL handshake always requires SID=0
    assert(PairedFrame::decode(raw,width,v)==PairedFrame::Error::OK);
    assert(freshB.accept(v)==S::Event::NONE);
    raw[6]=0;raw[5]=static_cast<uint8_t>(PairedFrame::Kind::CONTROL);
    assert(PairedFrame::decode(raw,width,v)==PairedFrame::Error::OK);
    assert(freshB.accept(v)==S::Event::NONE); // missing RESYNC flag
    // Invalid zero epoch must never emit a plausible discovery packet.
    S uninitialized(true,S::Identity());
    assert(!uninitialized.prepare(raw,width));

    // Session attempt cannot silently wrap.
    S exhausted(true,S::Identity(123,0xFFFF));
    assert(!exhausted.restart());
    exhausted.changeBoot(124);
    assert(exhausted.own().boot==124 && exhausted.own().attempt==0);
    assert(exhausted.restart() && exhausted.own().attempt==1);
    // Keep timer/retry policy outside the portable handshake layer.
}
