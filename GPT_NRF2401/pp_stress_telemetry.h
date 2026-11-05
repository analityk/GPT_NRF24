#ifndef GPT_NRF24_PP_STRESS_TELEMETRY_H
#define GPT_NRF24_PP_STRESS_TELEMETRY_H

#include <stdint.h>

// Per-device cumulative counters. Source and delivered bytes have different
// meanings by role: A generated/echoed; B forwarded/accepted into the echo
// queue. Neither is measured via the physical UART. All fields except SID
// and state must be monotonic modulo their unsigned integer width.
struct PpStressSnapshot {
    uint32_t sourceBytes, deliveredBytes, hardwareRetries, maxRetries;
    uint32_t echoGaps;
    uint16_t appRetries, duplicates, badFrames, blocked;
    uint16_t radioErrors, recoveries, sid;
    uint16_t acknowledgedBytes,linkGaps,linkStale,rxFifoFull,ackFifoFull;
    uint8_t outstanding;
    bool active;
    PpStressSnapshot()
        :sourceBytes(0),deliveredBytes(0),hardwareRetries(0),
         maxRetries(0),echoGaps(0),appRetries(0),duplicates(0),
         badFrames(0),blocked(0),radioErrors(0),recoveries(0),
         sid(0),acknowledgedBytes(0),linkGaps(0),linkStale(0),
         rxFifoFull(0),ackFifoFull(0),outstanding(0),active(false) {}
};

struct PpStressSample {
    uint32_t cps,sourceCps,hardwareRetries,maxRetries,echoGaps;
    uint16_t appRetries,duplicates,badFrames,blocked;
    uint16_t radioErrors,recoveries,sid;
    uint16_t acknowledgedBytes,linkGaps,linkStale,rxFifoFull,ackFifoFull;
    uint8_t outstanding;
    bool active;
};

// Reusable one-second measurement window for a 2 MHz modulo-2^32 clock.
// The actual elapsed time, not an assumed exact one-second timer interrupt,
// is used to normalize CPS. All differences are wrap-safe; 16-bit transport
// counters are sampled frequently enough not to wrap more than once/window.
class PpStressWindow {
public:
    PpStressWindow():lastTicks_(0),started_(false) {}
    void start(uint32_t ticks,const PpStressSnapshot& initial) {
        lastTicks_=ticks;last_=initial;started_=true;
    }
    bool take(uint32_t now,const PpStressSnapshot& current,
              PpStressSample& sample) {
        if(!started_) {start(now,current);return false;}
        const uint32_t elapsed=now-lastTicks_;
        if(elapsed<2000000UL)return false;
        const uint32_t elapsedMs=elapsed/2000UL;
        sample.cps=rate(current.deliveredBytes-last_.deliveredBytes,
                        elapsedMs);
        sample.sourceCps=rate(current.sourceBytes-last_.sourceBytes,
                              elapsedMs);
        sample.hardwareRetries=current.hardwareRetries-last_.hardwareRetries;
        sample.maxRetries=current.maxRetries-last_.maxRetries;
        sample.echoGaps=current.echoGaps-last_.echoGaps;
        sample.appRetries=static_cast<uint16_t>(current.appRetries-
                                                last_.appRetries);
        sample.duplicates=static_cast<uint16_t>(current.duplicates-
                                                last_.duplicates);
        sample.badFrames=static_cast<uint16_t>(current.badFrames-
                                               last_.badFrames);
        sample.blocked=static_cast<uint16_t>(current.blocked-last_.blocked);
        sample.radioErrors=static_cast<uint16_t>(current.radioErrors-
                                                 last_.radioErrors);
        sample.recoveries=static_cast<uint16_t>(current.recoveries-
                                                last_.recoveries);
        sample.acknowledgedBytes=static_cast<uint16_t>(
            current.acknowledgedBytes-last_.acknowledgedBytes);
        // Link sequence counters are scoped to one negotiated session,
        // unlike the lifetime counters in the PHY and transport.
        sample.linkGaps=(current.sid==last_.sid)?
            static_cast<uint16_t>(current.linkGaps-last_.linkGaps):
            current.linkGaps;
        sample.linkStale=(current.sid==last_.sid)?
            static_cast<uint16_t>(current.linkStale-last_.linkStale):
            current.linkStale;
        sample.rxFifoFull=static_cast<uint16_t>(
            current.rxFifoFull-last_.rxFifoFull);
        sample.ackFifoFull=static_cast<uint16_t>(
            current.ackFifoFull-last_.ackFifoFull);
        sample.outstanding=current.outstanding;
        sample.sid=current.sid;
        sample.active=current.active;
        lastTicks_=now;last_=current;
        return true;
    }
private:
    static uint32_t rate(uint32_t bytes,uint32_t elapsedMs) {
        // Dividing before multiplying prevents 32-bit overflow without
        // costly 64-bit division on the ATmega.
        return (bytes/elapsedMs)*1000UL+
               ((bytes%elapsedMs)*1000UL)/elapsedMs;
    }
    uint32_t lastTicks_;
    PpStressSnapshot last_;
    bool started_;
};

// Atomic, bounded ASCII telemetry output. A single line always fits the
// 127-byte usable UART ring if it is emitted, and never interleaves a binary
// transport segment. No printf/heap/AVR dependency. A failure drops only a
// diagnostic line, never a radio byte or a protocol ACK.
class PpStressLine {
public:
    PpStressLine():size_(0),valid_(true) {}
    void word(const char* value) {
        if(!value){valid_=false;return;}
        while(*value)character(*value++);
    }
    void decimal(uint32_t value) {
        char digits[10];uint8_t used=0;
        do {
            digits[used++]=static_cast<char>('0'+value%10UL);
            value/=10UL;
        }while(value);
        while(used)character(digits[--used]);
    }
    uint8_t length() const{
		return size_;
	}

    template<class Serial>
    bool emit(Serial& serial)const {
        return valid_ && serial.writeAll(
            reinterpret_cast<const uint8_t*>(data_),size_);
    }
private:
    void character(char c) {
        if(size_<sizeof(data_))data_[size_++]=c;
        else valid_=false;
    }
    char data_[127];
    uint8_t size_;
    bool valid_;
};

template<class Serial>
bool printPpStressLine(Serial& serial,bool leader,
                       const PpStressSample& s) {
    PpStressLine line;
    line.word(leader?"A cps=":"B cps=");line.decimal(s.cps);
    line.word(leader?" gen=":" fwd=");line.decimal(s.sourceCps);
    if(leader) {
        line.word(" ack=");line.decimal(s.acknowledgedBytes);
        line.word(" win=");line.decimal(s.outstanding);
        line.word(" arc=");line.decimal(s.hardwareRetries);
        line.word(" max=");line.decimal(s.maxRetries);
    }
    line.word(" ret=");line.decimal(s.appRetries);
    if(!leader){line.word(" dup=");line.decimal(s.duplicates);}
    line.word(" bad=");line.decimal(s.badFrames);
    if(!leader) {
        line.word(" lg=");line.decimal(s.linkGaps);
        line.word(" ls=");line.decimal(s.linkStale);
        line.word(" rf=");line.decimal(s.rxFifoFull);
        line.word(" af=");line.decimal(s.ackFifoFull);
    }
    if(!leader){line.word(" blk=");line.decimal(s.blocked);}
    if(leader){line.word(" gap=");line.decimal(s.echoGaps);}
    line.word(" err=");line.decimal(s.radioErrors);
    line.word(" rec=");line.decimal(s.recoveries);
    line.word(" sid=");line.decimal(s.sid);
    line.word(" up=");line.decimal(s.active?1U:0U);
    line.word("\r\n");
    return line.emit(serial);
}

#endif
