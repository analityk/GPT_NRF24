#ifndef GPT_NRF24_PP_UART_QUIET_DIAGNOSTIC_H
#define GPT_NRF24_PP_UART_QUIET_DIAGNOSTIC_H

#include <stdint.h>

// Diagnostics are published after RX/RF progress stops. No control bytes
// are inserted into the binary UART stream while a transfer is active.
// The counters are 16-bit; each measured burst must contain fewer than
// 65536 bytes per counter to avoid an ambiguous modulo-65536 delta.
#define PP_UART_DIAG_QUIET_TICKS 1600000UL
#define PP_UART_DIAG_POLL_TICKS 20000UL
#define PP_UART_DIAG_LINE_FREE_BYTES 112U
#define PP_UART_DIAG_TICKS_PER_MS 2000UL
#define PP_UART_DIAG_EMPTY_INPUT_FREE UINT8_MAX
#define PP_UART_DIAG_EVENT_COUNT 4U

// First observed occurrences, not an inferred causal ordering. Events in
// the same polling interval have the same timestamp. No ISR work/storage.
enum class PpUartDiagEventKind : uint8_t { MAX_RT,REPLAY,GAP,RX_FULL };

struct PpUartDiagEvent {
	uint16_t atMs,maxRt,retries,bad,rxFull;
	uint8_t outstanding,credit,freeInput;
};

struct PpUartDiagTrace {
	PpUartDiagEvent events[PP_UART_DIAG_EVENT_COUNT];
	uint8_t seen,minFree;
	uint16_t maxAckPauseMs;
	PpUartDiagTrace():events{},seen(0),minFree(PP_UART_DIAG_EMPTY_INPUT_FREE),
		maxAckPauseMs(0){
	}
};

// Reuse exactly this formatter in firmware and host length tests.
// Index zero is the summary; the remaining indices select event kinds.
template<class Line>
bool formatPpUartDiagTrace(Line& line,bool leader,
	const PpUartDiagTrace& trace,uint8_t index){
	if(index>PP_UART_DIAG_EVENT_COUNT){
		return false;
	}
	if(index==0){
		line.word(leader?"A wait minfree=":"B wait minfree=");
		line.decimal(trace.minFree);
		line.word(" ackpause_ms=");
		line.decimal(trace.maxAckPauseMs);
	} else {
		const uint8_t eventIndex=static_cast<uint8_t>(index-1U);
		if((trace.seen&static_cast<uint8_t>(1U<<eventIndex))==0){
			return false;
		}
		const PpUartDiagEvent& event=trace.events[eventIndex];
		line.word(leader?"A event k=":"B event k=");
		line.decimal(eventIndex);
		line.word(" ms=");
		line.decimal(event.atMs);
		line.word(" mx=");
		line.decimal(event.maxRt);
		line.word(" ret=");
		line.decimal(event.retries);
		line.word(" bad=");
		line.decimal(event.bad);
		line.word(" full=");
		line.decimal(event.rxFull);
		line.word(" win=");
		line.decimal(event.outstanding);
		line.word(" cr=");
		line.decimal(event.credit);
		line.word(" free=");
		line.decimal(event.freeInput);
	}
	line.word("\r\n");
	return true;
}

struct PpUartDiagSnapshot {
	uint16_t uartRx,uartTx,rxFull,rxOverrun,rxFraming;
	uint16_t acknowledged,accepted,retries,blocked,bad;
	uint16_t hwArc,maxRt,ackFifoFull,rxFifoFull;
	uint16_t sid;
	uint8_t outstanding,credit,freeInput;
	PpUartDiagSnapshot():
		uartRx(0),uartTx(0),rxFull(0),rxOverrun(0),rxFraming(0),
		acknowledged(0),accepted(0),retries(0),blocked(0),bad(0),
		hwArc(0),maxRt(0),ackFifoFull(0),rxFifoFull(0),
		sid(0),outstanding(0),credit(0),freeInput(0){
	}
};

struct PpUartDiagReport {
	PpUartDiagTrace trace;
	uint16_t uartRx,uartTx,rxFull,rxOverrun,rxFraming;
	uint16_t acknowledged,accepted,retries,blocked,bad;
	uint16_t hwArc,maxRt,ackFifoFull,rxFifoFull;
	uint16_t sid;
	uint8_t outstanding,credit,freeInput;
	PpUartDiagReport():
		uartRx(0),uartTx(0),rxFull(0),rxOverrun(0),rxFraming(0),
		acknowledged(0),accepted(0),retries(0),blocked(0),bad(0),
		hwArc(0),maxRt(0),ackFifoFull(0),rxFifoFull(0),
		sid(0),outstanding(0),credit(0),freeInput(0){
	}
};

// Reusable quiet-interval detector. It tracks bytes arriving at the UART,
// their loss at RX, data committed to the peer's UART, and application ACKs.
// Radio retries alone do not postpone a report for a stalled transport.
class PpUartQuietDiagnostic {
public:
	PpUartQuietDiagnostic():
		initialized_(false),armed_(false),lastActive_(0),
		started_(0),ackProgressAt_(0),hadBacklog_(false){
	}

	bool observe(uint32_t now,const PpUartDiagSnapshot& current,
		PpUartDiagReport& report){
		if(!initialized_){
			previous_=current;
			initialized_=true;
			return false;
		}
		const bool changed=current.uartRx!=previous_.uartRx ||
			current.rxFull!=previous_.rxFull ||
			current.rxOverrun!=previous_.rxOverrun ||
			current.acknowledged!=previous_.acknowledged ||
			current.accepted!=previous_.accepted;
		if(changed){
			if(!armed_){
				begin_=previous_;
				armed_=true;
				started_=now;
				ackProgressAt_=now;
				hadBacklog_=false;
				trace_=PpUartDiagTrace();
			}
			lastActive_=now;
		}
		if(armed_){
			observeTrace(now,current);
		}
		previous_=current;
		if(!armed_ || static_cast<uint32_t>(now-lastActive_)<
			PP_UART_DIAG_QUIET_TICKS){
			return false;
		}
		armed_=false;
		report.trace=trace_;
		report.uartRx=static_cast<uint16_t>(current.uartRx-begin_.uartRx);
		report.uartTx=static_cast<uint16_t>(current.uartTx-begin_.uartTx);
		report.rxFull=static_cast<uint16_t>(current.rxFull-begin_.rxFull);
		report.rxOverrun=static_cast<uint16_t>(
			current.rxOverrun-begin_.rxOverrun);
		report.rxFraming=static_cast<uint16_t>(
			current.rxFraming-begin_.rxFraming);
		report.acknowledged=static_cast<uint16_t>(
			current.acknowledged-begin_.acknowledged);
		report.accepted=static_cast<uint16_t>(
			current.accepted-begin_.accepted);
		report.retries=static_cast<uint16_t>(
			current.retries-begin_.retries);
		report.blocked=static_cast<uint16_t>(
			current.blocked-begin_.blocked);
		report.bad=static_cast<uint16_t>(current.bad-begin_.bad);
		report.hwArc=static_cast<uint16_t>(current.hwArc-begin_.hwArc);
		report.maxRt=static_cast<uint16_t>(current.maxRt-begin_.maxRt);
		report.ackFifoFull=static_cast<uint16_t>(
			current.ackFifoFull-begin_.ackFifoFull);
		report.rxFifoFull=static_cast<uint16_t>(
			current.rxFifoFull-begin_.rxFifoFull);
		report.sid=current.sid;
		report.outstanding=current.outstanding;
		report.credit=current.credit;
		report.freeInput=current.freeInput;
		return true;
	}

private:
	static uint16_t milliseconds(uint32_t ticks){
		const uint32_t ms=ticks/PP_UART_DIAG_TICKS_PER_MS;
		return static_cast<uint16_t>(ms>UINT16_MAX?UINT16_MAX:ms);
	}

	void record(PpUartDiagEventKind kind,uint32_t now,
		const PpUartDiagSnapshot& current){
		const uint8_t index=static_cast<uint8_t>(kind);
		const uint8_t bit=static_cast<uint8_t>(1U<<index);
		if((trace_.seen&bit)!=0){
			return;
		}
		trace_.seen=static_cast<uint8_t>(trace_.seen|bit);
		PpUartDiagEvent& event=trace_.events[index];
		event.atMs=milliseconds(static_cast<uint32_t>(now-started_));
		event.maxRt=static_cast<uint16_t>(current.maxRt-begin_.maxRt);
		event.retries=static_cast<uint16_t>(current.retries-begin_.retries);
		event.bad=static_cast<uint16_t>(current.bad-begin_.bad);
		event.rxFull=static_cast<uint16_t>(current.rxFull-begin_.rxFull);
		event.outstanding=current.outstanding;
		event.credit=current.credit;
		event.freeInput=current.freeInput;
	}

	void observeTrace(uint32_t now,const PpUartDiagSnapshot& current){
		if(current.freeInput<trace_.minFree){
			trace_.minFree=current.freeInput;
		}
		// A completed ACK interval must be measured BEFORE resetting its
		// origin; otherwise a stall ending between samples disappears.
		if(hadBacklog_){
			const uint16_t elapsed=milliseconds(
				static_cast<uint32_t>(now-ackProgressAt_));
			if(elapsed>trace_.maxAckPauseMs){
				trace_.maxAckPauseMs=elapsed;
			}
		}
		const bool backlog=current.outstanding!=0 ||
			current.freeInput<PP_UART_DIAG_EMPTY_INPUT_FREE;
		if(!hadBacklog_ || !backlog ||
			current.acknowledged!=previous_.acknowledged){
			ackProgressAt_=now;
		}
		hadBacklog_=backlog;
		if(current.maxRt!=previous_.maxRt){
			record(PpUartDiagEventKind::MAX_RT,now,current);
		}
		if(current.retries!=previous_.retries){
			record(PpUartDiagEventKind::REPLAY,now,current);
		}
		if(current.bad!=previous_.bad){
			record(PpUartDiagEventKind::GAP,now,current);
		}
		if(current.rxFull!=previous_.rxFull){
			record(PpUartDiagEventKind::RX_FULL,now,current);
		}
	}

	PpUartDiagSnapshot begin_,previous_;
	PpUartDiagTrace trace_;
	bool initialized_,armed_;
	uint32_t lastActive_;
	uint32_t started_,ackProgressAt_;
	bool hadBacklog_;
};

#endif
