#include "../GPT_NRF2401/pp_echo_ramp_port.h"
#include "../GPT_NRF2401/pp_recovery_control.h"
#include "../GPT_NRF2401/paired_session.h"
#include "../GPT_NRF2401/uart_reliable_stream.h"
#include <cassert>
#include <cstring>

struct NullConsole {
    bool read(uint8_t&){return false;}
    uint8_t freeSpace()const{return 127;}
    bool writeAll(const uint8_t*,uint8_t){return true;}
};

struct LoadedNode {
    bool leader;
    NullConsole console;
    PpEchoRampPort<NullConsole> port;
    PairedSession session;
    PairedLink link;
    UartReliableStream<PpEchoRampPort<NullConsole> > stream;
    PpRecoveryControl recovery;
    LoadedNode(bool a,uint32_t boot)
       :leader(a),port(console),
        session(a,PairedSession::Identity(boot,0)),link(a,0),
        stream(port),recovery(3200,2000000,
                     PpRecoveryControl::UncertainPolicy::RETRY_AT_LEAST_ONCE) {
        port.configure(a,0,PpEchoRampPort<NullConsole>::Mode::RADIO_STRESS);
        recovery.reset(0);
    }
    void tick(uint32_t now) {
        recovery.poll(now);
        port.service(now,recovery.canProduce());
    }
    void build(uint8_t (&packet)[32],uint8_t& width) {
        if(!session.active())assert(session.prepare(packet,width));
        else assert(stream.make(link,packet,width,recovery.canSendData()));
    }
    void accept(const uint8_t (&packet)[32],uint8_t width,uint32_t now) {
        PairedFrame::View v;
        assert(PairedFrame::decode(packet,width,v)==PairedFrame::Error::OK);
        const PairedSession::Event e=session.accept(v);
        if(e==PairedSession::Event::LOST){
            recovery.peerStateLost(now,stream.hasPending());
            link.configure(leader,0);
            stream.disconnect();
        } else if(e==PairedSession::Event::ESTABLISHED) {
            link.configure(leader,session.sid());
            stream.beginSession(session.sid());
            recovery.established(now);
        } else if(e!=PairedSession::Event::NONE && !session.active()) {
            link.configure(leader,0);
            stream.disconnect();
        }
        if(v.header.kind()==PairedFrame::Kind::CONTROL)return;
        if(!session.active()||v.header.sessionId!=session.sid())return;
        if(link.observeStream(v)==PairedLink::Receipt::WRONG_SESSION)return;
        recovery.validPeerFrame(now);
        (void)stream.accept(v);
    }
};

struct LoadedRadio {
    LoadedNode* a;
    LoadedNode* b;
    uint32_t now;
    uint8_t reply[32],replyWidth;
    LoadedRadio(LoadedNode& first,LoadedNode& second)
       :a(&first),b(&second),now(0),replyWidth(0) {
        b->build(reply,replyWidth);
    }
    void step(){
        now+=3200UL;
        a->tick(now);b->tick(now);
        uint8_t tx[32]={},oldReply[32]={},width=0;
        const uint8_t oldWidth=replyWidth;
        std::memcpy(oldReply,reply,oldWidth);
        a->build(tx,width);
        b->accept(tx,width,now);
        b->build(reply,replyWidth);
        a->accept(oldReply,oldWidth,now);
    }
    void settle(){
        for(unsigned i=0;i<100 && (!a->session.active()||
                                     !b->session.active());++i)step();
        assert(a->session.active() && b->session.active());
        assert(a->session.sid()==b->session.sid());
    }
    void bootB(LoadedNode& fresh) {
        b=&fresh;
        b->build(reply,replyWidth);
    }
    void bootA(LoadedNode& fresh) {
        // PRX keeps its ACK payload prepared for the OLD PTX session.
        // The fresh A has no pending stream data and must renegotiate.
        a=&fresh;
    }
};

int main(){
    LoadedNode a(true,101),bOld(false,201),bNew(false,202);
    LoadedRadio radio(a,bOld);
    radio.settle();
    for(unsigned i=0;i<2000;++i)radio.step();
    assert(a.port.echoed()>45000UL);
    assert(bOld.port.received()>45000UL);
    const uint16_t oldSid=a.session.sid();

    // B loses *all* RAM at 100% offered load, while A's four outstanding
    // DATA, unsent queue and sequence numbering survive unchanged.
    assert(a.stream.outstanding()>0);
    radio.bootB(bNew);
    radio.settle();
    assert(a.session.sid()!=oldSid);
    for(unsigned i=0;i<2000;++i)radio.step();
    // At ideal RF an old segment may be replayed during handshake, but
    // replay should not be a permanent steady-state tax on every packet.
    const uint16_t retries=a.stream.retransmits();
    const uint16_t oldFrames=bNew.stream.badFrames();
    const uint32_t stale=bNew.link.counters().stale;
    const uint32_t delivered=bNew.port.received();
    for(unsigned i=0;i<1000;++i)radio.step();
    assert(bNew.port.received()-delivered>20000UL);
    assert(static_cast<uint16_t>(a.stream.retransmits()-retries)<10);
    assert(static_cast<uint16_t>(bNew.stream.badFrames()-oldFrames)<10);
    assert(bNew.link.counters().stale-stale<10UL);

    // Independent reverse direction: B survives with ACK-Payload packets
    // and its pending echo, while A loses all RAM. This is NOT equivalent
    // to restarting B: the sender of buffered ACK-payload user DATA now
    // survives and the receiver starts empty.
    LoadedNode aNew(true,303);
    const uint16_t oldSid2=bNew.session.sid();
    radio.bootA(aNew);
    radio.settle();
    assert(aNew.session.sid()!=oldSid2);
    for(unsigned i=0;i<2000;++i)radio.step();
    const uint16_t bRetries=bNew.stream.retransmits();
    const uint16_t aBad=aNew.stream.badFrames();
    const uint32_t bDelivered=bNew.port.forwarded();
    for(unsigned i=0;i<1000;++i)radio.step();
    assert(bNew.port.forwarded()-bDelivered>20000UL);
    assert(static_cast<uint16_t>(bNew.stream.retransmits()-bRetries)<10);
    assert(static_cast<uint16_t>(aNew.stream.badFrames()-aBad)<10);
}
