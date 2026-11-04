#include "../GPT_NRF2401/pp_recovery_control.h"
#include "../GPT_NRF2401/host_flow_status.h"
#include "../GPT_NRF2401/uart_reliable_stream.h"
#include "../GPT_NRF2401/byte_ring.h"
#include <cassert>
#include <cstring>

struct RecoveryPort {
    ByteRing<128> source,sink;
    uint8_t available()const{return source.available();}
    uint8_t freeSpace()const{return sink.freeSpace();}
    bool read(uint8_t& value){return source.pop(value);}
    bool writeAll(const uint8_t* data,uint8_t n){
        if(sink.freeSpace()<n)return false;
        for(uint8_t i=0;i<n;++i)assert(sink.push(data[i]));
        return true;
    }
};
static void deliver(PairedLink& link,UartReliableStream<RecoveryPort>& to,
                    const uint8_t* bytes,uint8_t length){
    PairedFrame::View v;
    assert(PairedFrame::decode(bytes,length,v)==PairedFrame::Error::OK);
    assert(link.observeStream(v)!=PairedLink::Receipt::WRONG_SESSION);
    (void)to.accept(v);
}
int main(){
    typedef PpRecoveryControl R;
    R recovery(3200U,2000000U);
    recovery.reset(0);
    assert(recovery.state()==R::State::CONNECTING);
    recovery.established(1000);
    recovery.poll(10599); // 3 exchanges not yet elapsed
    assert(recovery.state()==R::State::HEALTHY);
    recovery.poll(10600);
    assert(recovery.state()==R::State::SUSPECT);
    assert(!recovery.canProduce());
    HostFlowStatus::Pair status=HostFlowStatus::snapshot(recovery,120,127);
    assert((status.tx&HostFlowStatus::STOP)!=0);
    assert((status.tx&63U)==0);
    assert((status.rx&HostFlowStatus::PEER_OFFLINE)!=0);
    assert((status.rx&63U)==31);
    assert(!HostFlowStatus::canCommit(recovery,127,1));
    recovery.poll(2001000);
    assert(recovery.state()==R::State::OFFLINE);
    recovery.validPeerFrame(2001001);
    assert(recovery.state()==R::State::HEALTHY);
    assert(recovery.warmRecoveries()==1);
    assert(HostFlowStatus::canCommit(recovery,7,7));
    assert(!HostFlowStatus::canCommit(recovery,7,8));
    assert(!recovery.warmExpired(2001001+9999999UL));
    recovery.poll(2001001+10000000UL);
    assert(recovery.warmExpired(2001001+10000000UL));
    recovery.abandonWarm(2001001+10000000UL,true);
    assert(recovery.expiredRecoveries()==1);
    assert(recovery.uncertainty() && recovery.held());
    recovery.acknowledgeUncertainty();
    recovery.retryUncertain();
    recovery.established(2001001+10000001UL);
    status=HostFlowStatus::snapshot(recovery,7,12);
    assert(status.tx==1);
    assert((status.rx&HostFlowStatus::RX_PRESSURE)!=0);

    // A peer identity change with unacknowledged data is NOT a normal
    // warm return. The default policy waits for an explicit host decision.
    recovery.peerStateLost(2002000,true);
    recovery.established(2003000);
    assert(recovery.uncertainty() && recovery.held());
    assert(!recovery.canSendData());
    status=HostFlowStatus::snapshot(recovery,120,127);
    assert((status.tx&(HostFlowStatus::STOP|HostFlowStatus::UNCERTAIN))==0xC0);
    recovery.retryUncertain();
    assert(recovery.canSendData() && recovery.uncertainty());
    recovery.acknowledgeUncertainty();
    assert(!recovery.uncertainty());

    // No 64-bit timers and no unsigned arithmetic pitfall at 2^32 wrap.
    recovery.reset(0xFFFFFF00UL);
    recovery.established(0xFFFFFF00UL);
    recovery.poll(0x0000247FUL);
    assert(recovery.state()==R::State::HEALTHY);
    recovery.poll(0x00002480UL);
    assert(recovery.state()==R::State::SUSPECT);

    // Warm outage: the DATA payload cache and negotiated SID must survive;
    // only IDLE probes are sent until an actual peer frame is validated.
    RecoveryPort aPort,bPort;
    PairedLink aLink(true,0x4567),bLink(false,0x4567);
    UartReliableStream<RecoveryPort> a(aPort),b(bPort);
    a.beginSession(0x4567);b.beginSession(0x4567);
    uint8_t bytes[48];
    for(uint8_t i=0;i<48;++i){bytes[i]=i;assert(aPort.source.push(i));}
    uint8_t packet[32],reply[32],width=0,replyWidth=0;
    // First exchange advertises B's credit, but is lost as an application
    // payload. B's next ACK carries the credit back to A.
    assert(b.make(bLink,reply,replyWidth));
    deliver(aLink,a,reply,replyWidth);
    assert(a.make(aLink,packet,width));
    assert(width==32);
    assert(a.make(aLink,packet,width));
    assert(width==32);
    assert(a.outstanding()==2 && aPort.source.available()==0);

    R active(3200U,2000000U);
    active.reset(123);active.established(123);
    active.poll(123+9600);
    for(uint8_t i=0;i<20;++i){
        assert(a.make(aLink,packet,width,active.canSendData()));
        PairedFrame::View v;
        assert(PairedFrame::decode(packet,width,v)==PairedFrame::Error::OK);
        assert(v.header.kind()==PairedFrame::Kind::IDLE);
        assert(a.outstanding()==2);
    }
    active.poll(123+2000000);
    assert(active.state()==R::State::OFFLINE);
    assert(aLink.sessionId()==0x4567);
    // Peer still has exactly the same RAM state: warm recovery, no rekey.
    assert(b.make(bLink,reply,replyWidth));
    deliver(aLink,a,reply,replyWidth);
    active.validPeerFrame(123+2000001);
    assert(active.canSendData() && active.warmRecoveries()==1);
    for(uint8_t i=0;i<28;++i){
        assert(b.make(bLink,reply,replyWidth));
        assert(a.make(aLink,packet,width,active.canSendData()));
        deliver(bLink,b,packet,width);
        deliver(aLink,a,reply,replyWidth);
    }
    assert(bPort.sink.available()==48);
    for(uint8_t i=0;i<48;++i){uint8_t value=0;
        assert(bPort.sink.pop(value) && value==bytes[i]);}
    assert(a.outstanding()==0 && a.acknowledgedBytes()==48);

    // Opt-in diagnostic replay can make progress after a COLD reset while
    // leaving an explicit latched duplicate-delivery warning for the host.
    R diagnostic(3200U,2000000U,R::UncertainPolicy::RETRY_AT_LEAST_ONCE);
    diagnostic.reset(0);diagnostic.established(1);
    diagnostic.peerStateLost(2,true);diagnostic.established(3);
    assert(diagnostic.canSendData() && diagnostic.uncertainty());
}
