#include "../GPT_NRF2401/pp_stress_telemetry.h"
#include <cassert>
#include <cstdint>
#include <cstring>

struct MockSerial {
    char data[128];
    uint8_t size;
    bool allow;
    MockSerial():size(0),allow(true) {}
    bool writeAll(const uint8_t* bytes,uint8_t count) {
        if(!allow || count>=sizeof(data))return false;
        std::memcpy(data,bytes,count);
        data[count]=0;
        size=count;
        return true;
    }
};

static void exactRatesAndCounters() {
    PpStressWindow window;
    PpStressSnapshot initial;
    initial.appRetries=65530U;
    initial.radioErrors=65535U;
    initial.recoveries=65535U;
    initial.rxFifoFull=65530U;
    initial.ackFifoFull=65535U;
    initial.acknowledgedBytes=65530U;
    initial.sourceBytes=0xFFFFFF00UL;
    window.start(123456U,initial);

    PpStressSnapshot next=initial;
    next.sourceBytes+=24000UL;
    next.deliveredBytes=20000UL;
    next.hardwareRetries=17;
    next.maxRetries=2;
    next.echoGaps=3;
    next.appRetries=5;
    next.duplicates=4;
    next.badFrames=6;
    next.blocked=7;
    next.radioErrors=1;
    next.recoveries=0;
    next.sid=0x7171U;
    next.active=true;
    next.acknowledgedBytes=5;
    next.outstanding=4;
    next.linkGaps=3;
    next.linkStale=2;
    next.rxFifoFull=4;
    next.ackFifoFull=1;
    PpStressSample s={};
    assert(!window.take(123456UL+1999999UL,next,s));
    assert(window.take(123456UL+2000000UL,next,s));
    assert(s.cps==20000UL && s.sourceCps==24000UL);
    assert(s.hardwareRetries==17UL && s.maxRetries==2UL);
    assert(s.appRetries==11 && s.radioErrors==2 && s.recoveries==1);
    assert(s.duplicates==4 && s.badFrames==6 && s.blocked==7);
    assert(s.echoGaps==3 && s.sid==0x7171U && s.active);
    assert(s.acknowledgedBytes==11 && s.outstanding==4);
    assert(s.linkGaps==3 && s.linkStale==2);
    assert(s.rxFifoFull==10 && s.ackFifoFull==2);

    next.deliveredBytes+=11000UL;
    next.sourceBytes+=20000UL;
    assert(window.take(123456UL+4200000UL,next,s));
    assert(s.cps==10000UL && s.sourceCps==18181UL);
    assert(s.hardwareRetries==0 && s.maxRetries==0);
    assert(s.appRetries==0 && s.echoGaps==0);
    assert(s.linkGaps==0 && s.linkStale==0);
    assert(s.acknowledgedBytes==0 && s.rxFifoFull==0);

    // A new negotiated session resets link-local sequence counters.
    // Do not mistake that reset for 65535 new missing frames.
    next.sid=0x7172U;
    next.linkGaps=1;
    next.linkStale=2;
    assert(window.take(123456UL+6200000UL,next,s));
    assert(s.linkGaps==1 && s.linkStale==2);
}

static void tickWrapAndText() {
    PpStressWindow window;
    PpStressSnapshot initial;
    const uint32_t start=0xFFF00000UL;
    window.start(start,initial);
    PpStressSnapshot next=initial;
    next.deliveredBytes=5000;
    next.sourceBytes=5500;
    next.hardwareRetries=12;
    next.maxRetries=4;
    next.appRetries=2;
    next.duplicates=3;
    next.echoGaps=1;
    next.sid=12345;
    next.active=true;
    next.outstanding=3;
    next.acknowledgedBytes=5500;
    next.linkGaps=9;
    next.linkStale=4;
    next.rxFifoFull=2;
    next.ackFifoFull=1;
    PpStressSample s={};
    assert(window.take(static_cast<uint32_t>(start+2000000UL),next,s));
    assert(s.cps==5000UL && s.sourceCps==5500UL);

    MockSerial serial;
    assert(printPpStressLine(serial,true,s));
    assert(std::strstr(serial.data,
                       "A cps=5000 gen=5500 ack=5500 win=3 arc=12 max=4")!=0);
    assert(std::strstr(serial.data,"ret=2 bad=0 gap=1")!=0);
    assert(std::strstr(serial.data,"sid=12345 up=1\r\n")!=0);
    assert(serial.size<127);

    assert(printPpStressLine(serial,false,s));
    assert(std::strstr(serial.data,"B cps=5000 fwd=5500 ret=2")!=0);
    assert(std::strstr(serial.data,"bad=0 lg=9 ls=4 rf=2 af=1")!=0);
    assert(std::strstr(serial.data," arc=")==0);
    assert(std::strstr(serial.data," max=")==0);
    assert(serial.size<127);
}

static void boundedAtomicOutput() {
    MockSerial serial;
    PpStressSample heavy={};
    heavy.cps=250000;heavy.sourceCps=250000;
    heavy.hardwareRetries=37500;heavy.maxRetries=2500;
    heavy.appRetries=3000;heavy.duplicates=3000;
    heavy.badFrames=3000;heavy.blocked=3000;
    heavy.echoGaps=250000;heavy.radioErrors=3000;
    heavy.recoveries=3000;heavy.sid=65535;heavy.active=true;
    heavy.acknowledgedBytes=30000;heavy.outstanding=4;
    heavy.linkGaps=3000;heavy.linkStale=3000;
    heavy.rxFifoFull=3000;heavy.ackFifoFull=3000;
    assert(printPpStressLine(serial,true,heavy));
    assert(serial.size<=127);
    assert(printPpStressLine(serial,false,heavy));
    assert(serial.size<=127);
    serial.size=0;
    PpStressLine line;
    for(unsigned i=0;i<200;++i)line.word("x");
    assert(!line.emit(serial) && serial.size==0);
    line=PpStressLine();
    line.decimal(4294967295UL);
    line.word("\r\n");
    assert(line.emit(serial));
    assert(std::strcmp(serial.data,"4294967295\r\n")==0);
    serial.allow=false;
    assert(!line.emit(serial));
}

int main() {
    exactRatesAndCounters();
    tickWrapAndText();
    boundedAtomicOutput();
}
