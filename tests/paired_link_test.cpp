#include "../GPT_NRF2401/paired_link.h"
#include <cassert>
#include <cstdint>
#include <cstring>
int main() {
  PairedLink a(true,0x1234),b(false,0x1234);
  uint8_t raw[32]={};uint8_t width=0;PairedFrame::View v;
  uint8_t data[24];for(unsigned i=0;i<24;++i)data[i]=static_cast<uint8_t>(i*17);
  assert(a.make(PairedFrame::Kind::DATA,PairedFrame::URGENT,4,data,24,raw,width)==PairedFrame::Error::OK);
  assert(width==32 && PairedFrame::decode(raw,width,v)==PairedFrame::Error::OK);
  assert(v.header.txSeq==0 && v.header.sessionId==0x1234 && v.header.flags==PairedFrame::URGENT);
  assert(v.length==24 && !std::memcmp(v.body,data,24));
  assert(b.receive(v)==PairedLink::Receipt::NEW_FRAME);
  assert(b.receive(v)==PairedLink::Receipt::DUPLICATE);
  assert(b.counters().duplicates==1 && b.peerCredit()==4);
  b.noteLocalDrop();
  assert(b.localDrops()==1);
  assert(b.make(PairedFrame::Kind::IDLE,0,3,0,0,raw,width)==PairedFrame::Error::OK);
  assert(width==8 && PairedFrame::decode(raw,width,v)==PairedFrame::Error::OK);
  assert(v.header.rxLast==0 && v.header.rxDrop==1 && v.header.rxCredit==3);
  assert(a.receive(v)==PairedLink::Receipt::NEW_FRAME && a.peerCredit()==3);
  assert(a.make(PairedFrame::Kind::IDLE,0,2,0,0,raw,width)==PairedFrame::Error::OK);
  assert(PairedFrame::decode(raw,width,v)==PairedFrame::Error::OK && v.header.rxLast==0);
  assert(b.receive(v)==PairedLink::Receipt::NEW_FRAME);
  assert(a.make(PairedFrame::Kind::IDLE,0,2,0,0,raw,width)==PairedFrame::Error::OK);
  assert(a.make(PairedFrame::Kind::IDLE,0,2,0,0,raw,width)==PairedFrame::Error::OK);
  assert(a.make(PairedFrame::Kind::IDLE,0,2,0,0,raw,width)==PairedFrame::Error::OK);
  assert(PairedFrame::decode(raw,width,v)==PairedFrame::Error::OK);
  assert(b.receive(v)==PairedLink::Receipt::NEW_FRAME && b.counters().gaps==2 && b.localLoss()==2);
  assert(b.make(PairedFrame::Kind::IDLE,0,1,0,0,raw,width)==PairedFrame::Error::OK);
  assert(PairedFrame::decode(raw,width,v)==PairedFrame::Error::OK && v.header.lossAB==2);
  assert(a.receive(v)==PairedLink::Receipt::NEW_FRAME && a.remoteLoss()==2);
  raw[6]=0xAA;
  assert(PairedFrame::decode(raw,width,v)==PairedFrame::Error::OK);
  assert(a.receive(v)==PairedLink::Receipt::WRONG_SESSION);
  assert(a.counters().foreign==1);
  uint8_t before[32];std::memcpy(before,raw,32);
  uint8_t w=99;PairedFrame::Header invalid;
  assert(PairedFrame::encode(invalid,data,25,raw,w)==PairedFrame::Error::BAD_LENGTH && w==99);
  assert(!std::memcmp(before,raw,32));
  assert(PairedFrame::decode(raw,7,v)==PairedFrame::Error::BAD_LENGTH);
  assert(PairedFrame::decode(raw,33,v)==PairedFrame::Error::BAD_LENGTH);
  invalid.flags=static_cast<uint8_t>(PairedFrame::Kind::IDLE);
  assert(PairedFrame::encode(invalid,data,1,raw,w)==PairedFrame::Error::BAD_TYPE);
  PairedLink wrapA(true,55),wrapB(false,55);
  for(unsigned i=0;i<260;++i){
    assert(wrapA.make(PairedFrame::Kind::IDLE,0,1,0,0,raw,width)==PairedFrame::Error::OK);
    assert(PairedFrame::decode(raw,width,v)==PairedFrame::Error::OK);
    assert(wrapB.receive(v)==PairedLink::Receipt::NEW_FRAME);
  }
  assert(wrapB.counters().gaps==0);
  assert(wrapA.make(PairedFrame::Kind::DATA,0,1,data,1,raw,width)==PairedFrame::Error::OK);
  assert(width==9);
  // Reusing a single object for different hardware roles resets statistics,
  // credit and the session epoch without carrying old RX metadata forward.
  a.configure(false,0xABCD);
  assert(a.sessionId()==0xABCD && a.peerCredit()==0 &&
         a.counters().rxFrames==0 && a.localLoss()==0);
  assert(a.make(PairedFrame::Kind::IDLE,0,1,0,0,raw,width)==PairedFrame::Error::OK);
  assert(raw[6]==0xCD && raw[7]==0xAB && raw[0]==0);
}
