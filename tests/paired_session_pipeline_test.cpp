#include "../GPT_NRF2401/paired_session.h"
#include "../GPT_NRF2401/paired_link.h"
#include <cassert>
#include <cstdint>
#include <cstring>

// Host-level transceiver model. B's hardware ACK payload is queued BEFORE
// A sends the frame that triggers it. A may therefore receive a reply from
// an earlier state/epoch; the model explicitly preserves that delay.
// Exercises actual production PairedSession/PairedFrame/PairedLink code.
struct Endpoint {
    bool leader;
    PairedSession session;
    PairedLink link;
    Endpoint(bool role,uint32_t epoch)
       :leader(role),session(role,PairedSession::Identity(epoch,0)),
        link(role,0){}
    void reboot(uint32_t epoch) {
        session.configure(leader,PairedSession::Identity(epoch,0));
        link.configure(leader,0);
    }
    bool build(uint8_t (&out)[32],uint8_t& width) {
        if(!session.active())return session.prepare(out,width);
        if(link.sessionId()!=session.sid())
            link.configure(leader,session.sid());
        return link.make(PairedFrame::Kind::IDLE,0,1,0,0,out,width)
               ==PairedFrame::Error::OK;
    }
    PairedSession::Event accept(const uint8_t (&frame)[32],uint8_t width) {
        PairedFrame::View view;
        if(PairedFrame::decode(frame,width,view)!=PairedFrame::Error::OK)
            return PairedSession::Event::NONE;
        const PairedSession::Event result=session.accept(view);
        if(result==PairedSession::Event::LOST)
            link.configure(leader,0);
        else if(result==PairedSession::Event::ESTABLISHED)
            link.configure(leader,session.sid());
        if(view.header.kind()!=PairedFrame::Kind::CONTROL &&
           session.active() && view.header.sessionId==session.sid())
            (void)link.receive(view);
        return result;
    }
};

// PRX rebuilds queued ACK when a handshake epoch/phase changes. This
// abstracts the firmware's replacePrxReply() operation (flush and reload).
struct Exchange {
    Endpoint a,b;
    uint8_t bQueued[32]={};
    uint8_t bWidth=0;
    unsigned frames=0;
    unsigned aData=0,bData=0;
    Exchange():a(true,10),b(false,20){ assert(b.build(bQueued,bWidth));}
    void trigger(bool dropForward=false,bool dropAck=false) {
        uint8_t outbound[32]={},oldReply[32]={},txWidth=0;
        assert(a.build(outbound,txWidth));
        const uint8_t oldWidth=bWidth;
        std::memcpy(oldReply,bQueued,32);
        PairedFrame::View view;
        assert(PairedFrame::decode(outbound,txWidth,view)==PairedFrame::Error::OK);
        if(view.header.kind()==PairedFrame::Kind::IDLE)++aData;
        if(!dropForward) {
            const PairedSession::Event event=b.accept(outbound,txWidth);
            // When the PRX consumes a queued ACK, it must stage its next
            // response. On an epoch transition, it first flushes old state.
            if(event!=PairedSession::Event::NONE) {
                // The response in oldReply is still the one transmitted by
                // this transaction, not the new one we are preparing.
            }
            assert(b.build(bQueued,bWidth));
        }
        if(!dropAck && !dropForward){
            assert(PairedFrame::decode(oldReply,oldWidth,view)
                   ==PairedFrame::Error::OK);
            if(view.header.kind()==PairedFrame::Kind::IDLE)++bData;
            (void)a.accept(oldReply,oldWidth);
        }
        ++frames;
    }
    void settle(unsigned limit=32){
        for(unsigned i=0;i<limit && (!a.session.active() ||
                                    !b.session.active());++i)
            trigger();
        assert(a.session.active() && b.session.active());
        assert(a.session.sid()==b.session.sid());
        assert(a.session.sid()!=0);
    }
    void rebootB(uint32_t epoch){
        b.reboot(epoch);
        assert(b.build(bQueued,bWidth)); // old queued ACK flushed on reboot
    }
};
int main() {
    Exchange x;
    x.settle();
    const uint16_t first=x.a.session.sid();
    for(unsigned i=0;i<300;++i)x.trigger();
    assert(x.a.session.active() && x.b.session.active());
    assert(x.aData>256 && x.bData>256);
    assert(x.a.link.counters().gaps==0 &&
           x.b.link.counters().gaps==0);

    // B boots a new epoch while A is still transmitting old-session IDLE.
    // A must detect BEACON and negotiate a new SID, never stay in old ACTIVE.
    x.rebootB(21);
    x.settle();
    assert(x.a.session.sid()!=first);
    const uint16_t afterB=x.a.session.sid();

    // A MCU reboots. The first ACK it receives was queued by the old B
    // ACTIVE session; it must be rejected by the new A, and B has to see
    // a new HELLO epoch to transition out of ACTIVE.
    x.a.reboot(11);
    x.settle();
    assert(x.a.session.sid()!=afterB);

    // Lost ACK responses in DISCOVER/WELCOME must not deadlock. The B
    // reply is prepared in advance and cannot acknowledge current HELLO.
    Exchange lost;
    lost.trigger(false,true);
    lost.trigger(false,true);
    lost.trigger(false,true);
    lost.settle();
    assert(lost.a.session.active() && lost.b.session.active());

    // A restarts negotiation after a timeout with a fresh attempt nonce.
    const uint16_t oldSid=lost.a.session.sid();
    assert(lost.a.session.restart());
    lost.a.link.configure(true,0);
    lost.settle();
    assert(lost.a.session.sid()!=oldSid);

    // A reset and B reset simultaneously, with two fresh boot epochs.
    lost.a.reboot(50);
    lost.rebootB(60);
    lost.settle();
    assert(lost.a.session.sid()==lost.b.session.sid());

    // Physical non-delivery of a forward packet leaves the PRX's queued
    // reply untouched. Once contact is restored, normal handshake resumes.
    Exchange disconnected;
    for(unsigned i=0;i<8;++i)disconnected.trigger(true,false);
    disconnected.settle();
}
