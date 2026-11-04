#include "../GPT_NRF2401/burst20_report.h"
#include "../GPT_NRF2401/paired_frame.h"
#include <cassert>
#include <cstdint>
#include <cstring>
int main() {
    uint8_t bytes[Burst20Report::BODY_SIZE]={};
    Burst20Report::Summary v;
    v.share=Burst20Schedule::Share::A5_B15;
    v.owner=Burst20Schedule::Owner::B;
    v.flags=Burst20Report::OVERFLOW | Burst20Report::MISSED_REPORT;
    v.cycle=0xFEDC;
    v.firstSeq=0xE7;
    v.dataPackets=12;v.dataBytes=277;
    v.dataCrc32=0x1234ABCDUL;
    v.peerCycle=0xDCBA;
    v.peerBitmap=0xA0000001UL;
    v.acceptedPeerPackets=2;v.missingPeerPackets=7;v.peerCredit=5;
    assert(Burst20Report::encode(v,bytes,sizeof(bytes)));
    assert(bytes[0]==1 && bytes[1]==1 && bytes[2]==1 && bytes[3]==5);
    assert(bytes[4]==0xDC && bytes[5]==0xFE);
    assert(bytes[8]==0x15 && bytes[9]==0x01);
    assert(bytes[10]==0xCD && bytes[11]==0xAB &&
           bytes[12]==0x34 && bytes[13]==0x12);
    assert(bytes[16]==0x01 && bytes[19]==0xA0 && bytes[23]==0);
    Burst20Report::Summary result;
    assert(Burst20Report::decode(bytes,sizeof(bytes),result));
    assert(result.share==v.share && result.owner==v.owner &&
           result.flags==v.flags && result.cycle==v.cycle &&
           result.firstSeq==v.firstSeq && result.dataPackets==v.dataPackets &&
           result.dataBytes==v.dataBytes && result.dataCrc32==v.dataCrc32 &&
           result.peerCycle==v.peerCycle && result.peerBitmap==v.peerBitmap &&
           result.acceptedPeerPackets==v.acceptedPeerPackets &&
           result.missingPeerPackets==v.missingPeerPackets &&
           result.peerCredit==v.peerCredit);
    uint8_t RF[32]={},rfWidth=0;
    PairedFrame::Header h;
    h.sessionId=0x9876;
    h.flags=static_cast<uint8_t>(PairedFrame::Kind::CONTROL);
    assert(PairedFrame::encode(h,bytes,24,RF,rfWidth)==PairedFrame::Error::OK);
    assert(rfWidth==32);
    PairedFrame::View decoded;
    assert(PairedFrame::decode(RF,rfWidth,decoded)==PairedFrame::Error::OK);
    assert(decoded.header.kind()==PairedFrame::Kind::CONTROL &&
           decoded.length==24);
    Burst20Report::Summary recovered;
    assert(Burst20Report::decode(decoded.body,decoded.length,recovered));
    assert(recovered.dataCrc32==0x1234ABCDUL);

    // Reject malformed inputs without mutating the receiving summary.
    result.cycle=0x1337;
    bytes[0]=2;
    assert(!Burst20Report::decode(bytes,24,result));
    assert(result.cycle==0x1337);
    bytes[0]=1;
    bytes[23]=1;
    assert(!Burst20Report::decode(bytes,24,result));
    bytes[23]=0;
    assert(!Burst20Report::decode(bytes,23,result));
    v.dataPackets=33;
    uint8_t before[24];std::memcpy(before,bytes,24);
    assert(!Burst20Report::encode(v,bytes,24));
    assert(std::memcmp(before,bytes,24)==0);
    v.dataPackets=12;
    v.dataBytes=289;
    assert(!Burst20Report::encode(v,bytes,24));
    v.dataBytes=277;
    assert(!Burst20Report::encode(v,bytes,23));

    // Standard known check: CRC32("123456789")=CBF43926.
    Burst20Report::RollingCrc32 crc;
    const uint8_t pattern[]={'1','2','3','4','5','6','7','8','9'};
    crc.add(pattern,4);
    crc.add(pattern+4,5);
    assert(crc.value()==0xCBF43926UL);
    crc.reset();
    crc.add(pattern,9);
    assert(crc.value()==0xCBF43926UL);

    // Receipt bitmap is bounded, idempotent and does not count a repeat.
    Burst20Report::Receipt rx;
    assert(rx.accept(0) && rx.accept(15) && rx.accept(31));
    assert(!rx.accept(15) && !rx.accept(32));
    assert(rx.bitmap()==0x80008001UL);
    assert(rx.accepted()==3 && rx.missing(5)==2);
    rx.reset();
    assert(rx.bitmap()==0 && rx.accepted()==0);
}
