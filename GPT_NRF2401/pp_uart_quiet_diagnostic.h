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

struct PpUartDiagSnapshot {
	uint16_t uartRx,uartTx,rxFull,rxOverrun,rxFraming;
	uint16_t acknowledged,accepted,retries,blocked,bad;
	uint16_t sid;
	uint8_t outstanding,credit,freeInput;
	PpUartDiagSnapshot():
		uartRx(0),uartTx(0),rxFull(0),rxOverrun(0),rxFraming(0),
		acknowledged(0),accepted(0),retries(0),blocked(0),bad(0),
		sid(0),outstanding(0),credit(0),freeInput(0){
	}
};

struct PpUartDiagReport {
	uint16_t uartRx,uartTx,rxFull,rxOverrun,rxFraming;
	uint16_t acknowledged,accepted,retries,blocked,bad;
	uint16_t sid;
	uint8_t outstanding,credit,freeInput;
	PpUartDiagReport():
		uartRx(0),uartTx(0),rxFull(0),rxOverrun(0),rxFraming(0),
		acknowledged(0),accepted(0),retries(0),blocked(0),bad(0),
		sid(0),outstanding(0),credit(0),freeInput(0){
	}
};

// Reusable quiet-interval detector. It tracks bytes arriving at the UART,
// their loss at RX, data committed to the peer's UART, and application ACKs.
// Radio retries alone do not postpone a report for a stalled transport.
class PpUartQuietDiagnostic {
public:
	PpUartQuietDiagnostic():
		initialized_(false),armed_(false),lastActive_(0){
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
			}
			lastActive_=now;
		}
		previous_=current;
		if(!armed_ || static_cast<uint32_t>(now-lastActive_)<
			PP_UART_DIAG_QUIET_TICKS){
			return false;
		}
		armed_=false;
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
		report.sid=current.sid;
		report.outstanding=current.outstanding;
		report.credit=current.credit;
		report.freeInput=current.freeInput;
		return true;
	}

private:
	PpUartDiagSnapshot begin_,previous_;
	bool initialized_,armed_;
	uint32_t lastActive_;
};

#endif
