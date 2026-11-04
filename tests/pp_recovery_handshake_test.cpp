#include "../GPT_NRF2401/pp_recovery_control.h"
#include "../GPT_NRF2401/paired_session.h"
#include "../GPT_NRF2401/uart_reliable_stream.h"
#include "../GPT_NRF2401/byte_ring.h"
#include <cassert>
#include <cstring>

struct LocalPort {
    ByteRing<128> input,output;
    uint8_t available()const{return input.available();}
    uint8_t freeSpace()const{return output.freeSpace();}
    bool read(uint8_t& value){return input.pop(value);}
    bool writeAll(const uint8_t* data,uint8_t n){
        if(output.freeSpace()<n)return false;
        for(uint8_t i=0;i<n;++i)assert(output.push(data[i]));
        return true;
    }
};

struct Device {
    bool isA;
    PairedSession session;
    PairedLink link;
    LocalPort port;
    UartReliableStream<LocalPort> stream;
    PpRecoveryControl recovery;
    Device(bool a,uint32_t boot)
       :isA(a),session(a,PairedSession::Identity(boot,0)),link(a,0),
        stream(port),recovery(3200,2000000,
                   PpRecoveryControl::UncertainPolicy::RETRY_AT_LEAST_ONCE) {
        recovery.reset(0);
    }
    void reboot(uint32_t boot,uint32_t now){
        session.configure(isA,PairedSession::Identity(boot,0));
        link.configure(isA,0);
        // Simulate loss of volatile source/sink RAM on power cycle.
        port.input.reset();
        port.output.reset();
        stream.disconnect();
        recovery.reset(now);
    }
    void service(uint32_t now) {
        recovery.poll(now);
        if(session.active() && recovery.warmExpired(now)){
            recovery.abandonWarm(now,stream.hasPending());
            assert(session.restart());
            link.configure(isA,0);
            stream.disconnect();
        }
    }
    void build(uint8_t (&frame)[32],uint8_t& width){
        if(session.active()){
            assert(stream.make(link,frame,width,recovery.canSendData()));
        } else assert(session.prepare(frame,width));
    }
    void accept(const uint8_t (&frame)[32],uint8_t width,uint32_t now){
        PairedFrame::View view;
        assert(PairedFrame::decode(frame,width,view)==PairedFrame::Error::OK);
        const PairedSession::Event e=session.accept(view);
        if(e==PairedSession::Event::LOST){
            recovery.peerStateLost(now,stream.hasPending());
            link.configure(isA,0);
            stream.disconnect();
        }else if(e==PairedSession::Event::ESTABLISHED){
            link.configure(isA,session.sid());
            stream.beginSession(session.sid());
            recovery.established(now);
        }
        if(view.header.kind()==PairedFrame::Kind::CONTROL)return;
        if(!session.active()||view.header.sessionId!=session.sid())return;
        if(link.observeStream(view)==PairedLink::Receipt::WRONG_SESSION)return;
        recovery.validPeerFrame(now);
        stream.accept(view);
    }
};

struct TestRadio {
    Device a,b;
    uint8_t queued[32];
    uint8_t queuedWidth;
    uint32_t now;
    TestRadio():a(true,101),b(false,201),queuedWidth(0),now(0){
        b.build(queued,queuedWidth);
    }
    void step(bool linkAvailable=true){
        now+=3200U;
        const bool wasActive=b.session.active();
        a.service(now);b.service(now);
        if(wasActive && !b.session.active())
            b.build(queued,queuedWidth); // PRX flush and reload new BEACON
        uint8_t sent[32],prior[32],w=0;
        const uint8_t pw=queuedWidth;
        std::memcpy(prior,queued,pw);
        a.build(sent,w);
        if(!linkAvailable)return;
        b.accept(sent,w,now);
        b.build(queued,queuedWidth); // preloaded ACK for the NEXT exchange
        a.accept(prior,pw,now);
    }
    void settle(){
        for(unsigned i=0;i<40 && (!a.session.active()||
                                     !b.session.active());++i)step();
        assert(a.session.active() && b.session.active());
        assert(a.session.sid()==b.session.sid());
    }
    void rebootB(uint32_t boot){
        b.reboot(boot,now);
        b.build(queued,queuedWidth);
    }
};

int main(){
    TestRadio x;
    x.settle();
    for(unsigned i=0;i<4;++i)x.step(); // first delayed active RX_CREDIT
    const uint16_t oldSid=x.a.session.sid();
    for(uint8_t i=0;i<24;++i)assert(x.a.port.input.push(i));
    // Packet queued locally but not delivered over RF.
    x.step(false);
    assert(x.a.stream.outstanding()==1);
    for(unsigned i=0;i<20;++i)x.step(false);
    assert(x.a.recovery.state()==PpRecoveryControl::State::SUSPECT);
    assert(x.b.recovery.state()==PpRecoveryControl::State::SUSPECT);
    assert(x.a.session.sid()==oldSid && x.b.session.sid()==oldSid);
    for(unsigned i=0;i<24;++i)x.step();
    assert(x.a.recovery.warmRecoveries()>0);
    assert(x.b.recovery.warmRecoveries()>0);
    assert(x.a.session.sid()==oldSid && x.b.session.sid()==oldSid);
    assert(x.a.stream.outstanding()==0);
    assert(x.b.port.output.available()==24);

    // A full B reboot is identified by a NEW BEACON, not by an elapsed
    // timer. The outstanding bytes are preserved, but are marked UNCERTAIN.
    for(uint8_t i=0;i<24;++i)assert(x.a.port.input.push(i+24U));
    x.step(false);
    assert(x.a.stream.outstanding()==1);
    x.rebootB(202);
    x.settle();
    assert(x.a.session.sid()!=oldSid);
    assert(x.a.recovery.uncertainty());
    for(unsigned i=0;i<24;++i)x.step();
    assert(x.b.port.output.available()==24);
    assert(x.a.stream.outstanding()==0);

    // Now B has one unacknowledged ACK Payload segment. Only A reboots.
    for(uint8_t i=0;i<24;++i)assert(x.b.port.input.push(i+75U));
    x.b.build(x.queued,x.queuedWidth);
    assert(x.b.stream.outstanding()==1);
    x.a.reboot(102,x.now);
    x.settle();
    assert(x.b.recovery.uncertainty());
    assert(x.b.recovery.coldRecoveries()>=1);
    for(unsigned i=0;i<24;++i)x.step();
    assert(x.b.stream.outstanding()==0);
    assert(x.a.port.output.available()==24);

    // Both reboot: no stale wire identity survives either device.
    x.a.reboot(103,x.now);
    x.rebootB(203);
    x.settle();
    assert(x.a.session.peer().boot==203);
    assert(x.b.session.peer().boot==103);

    // Five seconds of genuine silence exhaust the warm-state budget on
    // BOTH sides. Rejoin still works, with an explicit ambiguous-delivery
    // flag if cached in-flight DATA must be retried in the new session.
    TestRadio expiry;
    expiry.settle();
    for(unsigned i=0;i<4;++i)expiry.step();
    const uint16_t beforeExpiry=expiry.a.session.sid();
    for(uint8_t i=0;i<24;++i)assert(expiry.a.port.input.push(i));
    expiry.step(false);
    assert(expiry.a.stream.outstanding()==1);
    for(unsigned i=0;i<3300;++i)expiry.step(false);
    assert(expiry.a.recovery.expiredRecoveries()==1);
    assert(expiry.b.recovery.expiredRecoveries()==1);
    assert(!expiry.a.session.active() && !expiry.b.session.active());
    expiry.settle();
    assert(expiry.a.session.sid()!=beforeExpiry);
    assert(expiry.a.recovery.uncertainty());
    for(unsigned i=0;i<32;++i)expiry.step();
    assert(expiry.a.stream.outstanding()==0);
    assert(expiry.b.port.output.available()==24);
}
