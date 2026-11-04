#include "../GPT_NRF2401/paired_link.h"
#include <cassert>
#include <cstdint>
#include <cstring>

static PairedFrame::View parse(const uint8_t* raw,uint8_t width) {
    PairedFrame::View v;
    assert(PairedFrame::decode(raw,width,v)==PairedFrame::Error::OK);
    return v;
}
int main() {
    PairedLink a(true,0x1234),b(false,0x1234);
    uint8_t raw[32]={},width=0,payload[24];
    for(uint8_t i=0;i<24;++i)payload[i]=static_cast<uint8_t>(i*17U);

    // DATA: exactly 8 bytes of link header and 24 raw payload bytes.
    assert(a.makeStream(0,0xFF,4,payload,24,raw,width)
           ==PairedFrame::Error::OK);
    PairedFrame::View v=parse(raw,width);
    assert(width==32 && v.length==24);
    assert(v.header.txSeq==0 && v.header.rxLast==0xFF);
    assert(v.header.rxCredit==4 && v.header.sessionId==0x1234);
    assert(std::memcmp(v.body,payload,24)==0);
    assert(b.observeStream(v)==PairedLink::Receipt::NEW_FRAME);
    assert(b.peerCredit()==4 && b.counters().rxFrames==1);

    // Duplicate DATA may still bring a FRESH cumulative ACK and credit.
    assert(a.makeStream(0,12,2,payload,24,raw,width)
           ==PairedFrame::Error::OK);
    v=parse(raw,width);
    assert(b.observeStream(v)==PairedLink::Receipt::DUPLICATE);
    assert(b.peerCredit()==2 && b.counters().duplicates==1);
    assert(v.header.rxLast==12 && v.body[0]==payload[0]);

    // IDLE contains 8 bytes total, still advertises ACK and receiver credit.
    b.noteLocalDrop();
    assert(b.localDrops()==1);
    assert(b.makeStream(0,0,3,0,0,raw,width)==PairedFrame::Error::OK);
    v=parse(raw,width);
    assert(width==8 && v.length==0);
    assert(v.header.kind()==PairedFrame::Kind::IDLE);
    assert(v.header.rxLast==0 && v.header.rxDrop==1);
    assert(a.observeStream(v)==PairedLink::Receipt::NEW_FRAME);
    assert(a.peerCredit()==3);
    const uint32_t prior=a.counters().rxFrames;
    // Repeated empty status does not consume the DATA sequence space.
    assert(a.observeStream(v)==PairedLink::Receipt::NEW_FRAME);
    assert(a.counters().rxFrames==prior+1 && a.counters().gaps==0);

    // Logical DATA gaps are diagnosed separately from raw RF exchanges.
    assert(a.makeStream(4,0,2,payload,1,raw,width)==PairedFrame::Error::OK);
    v=parse(raw,width);
    assert(b.observeStream(v)==PairedLink::Receipt::NEW_FRAME);
    assert(b.counters().gaps==3 && b.localLoss()==3);
    assert(b.makeStream(0,4,1,0,0,raw,width)==PairedFrame::Error::OK);
    v=parse(raw,width);
    assert(v.header.lossAB==3 && v.header.rxLast==4);
    assert(a.observeStream(v)==PairedLink::Receipt::NEW_FRAME);
    assert(a.remoteLoss()==3);

    // Physical PTX ARC can report retries even when no data gap occurs.
    a.notePtxRetries(5);
    assert(a.makeStream(5,0,2,payload,2,raw,width)==PairedFrame::Error::OK);
    v=parse(raw,width);
    assert(v.header.arcLast==5);
    assert(b.observeStream(v)==PairedLink::Receipt::NEW_FRAME);

    // Any other session is rejected before changing credit or counters.
    const uint8_t credit=b.peerCredit();
    raw[6]=0xAA;
    v=parse(raw,width);
    assert(b.observeStream(v)==PairedLink::Receipt::WRONG_SESSION);
    assert(b.peerCredit()==credit && b.counters().foreign==1);

    // Bad encodes must leave caller's buffer and output width unchanged.
    uint8_t snapshot[32];std::memcpy(snapshot,raw,sizeof(raw));
    PairedFrame::Header h;
    uint8_t w=99;
    assert(PairedFrame::encode(h,payload,25,raw,w)==PairedFrame::Error::BAD_LENGTH);
    assert(w==99 && std::memcmp(snapshot,raw,32)==0);
    assert(PairedFrame::decode(raw,7,v)==PairedFrame::Error::BAD_LENGTH);
    assert(PairedFrame::decode(raw,33,v)==PairedFrame::Error::BAD_LENGTH);
    h.flags=static_cast<uint8_t>(PairedFrame::Kind::IDLE);
    assert(PairedFrame::encode(h,payload,1,raw,w)==PairedFrame::Error::BAD_TYPE);

    // Variable lengths 0..24, with no extra transport metadata in BODY.
    for(uint8_t n=0;n<=24;++n) {
        h.flags=static_cast<uint8_t>(n?PairedFrame::Kind::DATA:
                                      PairedFrame::Kind::IDLE);
        h.sessionId=0xCAFE;
        assert(PairedFrame::encode(h,payload,n,raw,w)==PairedFrame::Error::OK);
        v=parse(raw,w);
        assert(w==static_cast<uint8_t>(8+n) && v.length==n);
        assert(std::memcmp(v.body,payload,n)==0);
    }

    // Eight-bit stream sequencing wraps, while IDLE frames do not move it.
    PairedLink tx(true,55),rx(false,55);
    for(unsigned i=0;i<260;++i) {
        const uint8_t octet=static_cast<uint8_t>(i);
        assert(tx.makeStream(octet,0xFF,1,&octet,1,raw,width)
               ==PairedFrame::Error::OK);
        v=parse(raw,width);
        assert(rx.observeStream(v)==PairedLink::Receipt::NEW_FRAME);
        assert(tx.makeStream(static_cast<uint8_t>(i+1U),0xFF,1,0,0,raw,width)
               ==PairedFrame::Error::OK);
        v=parse(raw,width);
        assert(rx.observeStream(v)==PairedLink::Receipt::NEW_FRAME);
    }
    assert(rx.counters().gaps==0 && rx.counters().rxFrames==520);

    // Role/session reconfiguration resets only link state.
    a.configure(false,0xABCD);
    assert(a.sessionId()==0xABCD && a.peerCredit()==0);
    assert(a.counters().rxFrames==0 && a.localLoss()==0);
    assert(a.makeStream(0,0xFF,1,0,0,raw,width)==PairedFrame::Error::OK);
    assert(width==8 && raw[6]==0xCD && raw[7]==0xAB);
}
