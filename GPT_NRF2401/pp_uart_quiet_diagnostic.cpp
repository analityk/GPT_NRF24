#include "pp_uart_quiet_diagnostic.h"

PpUartDiagTrace::PpUartDiagTrace()
	: events{},
	  seen(0),
	  minFree(PP_UART_DIAG_EMPTY_INPUT_FREE),
	  maxAckPauseMs(0) {
}

PpUartDiagSnapshot::PpUartDiagSnapshot()
	: uartRx(0),
	  uartTx(0),
	  rxFull(0),
	  rxOverrun(0),
	  rxFraming(0),
	  acknowledged(0),
	  accepted(0),
	  retries(0),
	  blocked(0),
	  bad(0),
	  hwArc(0),
	  maxRt(0),
	  ackFifoFull(0),
	  rxFifoFull(0),
	  sid(0),
	  outstanding(0),
	  credit(0),
	  freeInput(0) {
}

PpUartDiagReport::PpUartDiagReport()
	: uartRx(0),
	  uartTx(0),
	  rxFull(0),
	  rxOverrun(0),
	  rxFraming(0),
	  acknowledged(0),
	  accepted(0),
	  retries(0),
	  blocked(0),
	  bad(0),
	  hwArc(0),
	  maxRt(0),
	  ackFifoFull(0),
	  rxFifoFull(0),
	  sid(0),
	  outstanding(0),
	  credit(0),
	  freeInput(0) {
}

PpUartQuietDiagnostic::PpUartQuietDiagnostic()
	: initialized_(false),
	  armed_(false),
	  lastActive_(0),
	  started_(0),
	  ackProgressAt_(0),
	  hadBacklog_(false) {
}

bool PpUartQuietDiagnostic::observe(
	uint32_t now, const PpUartDiagSnapshot& current, PpUartDiagReport& report) {
	if (!initialized_) {
		previous_ = current;
		initialized_ = true;
		return false;
	}
	const bool changed = current.uartRx != previous_.uartRx || current.rxFull != previous_.rxFull ||
						 current.rxOverrun != previous_.rxOverrun ||
						 current.acknowledged != previous_.acknowledged ||
						 current.accepted != previous_.accepted;
	if (changed) {
		if (!armed_) {
			begin_ = previous_;
			armed_ = true;
			started_ = now;
			ackProgressAt_ = now;
			hadBacklog_ = false;
			trace_ = PpUartDiagTrace();
		}
		lastActive_ = now;
	}
	if (armed_) {
		observeTrace(now, current);
	}
	previous_ = current;
	if (!armed_ || (now - lastActive_) < PP_UART_DIAG_QUIET_TICKS) {
		return false;
	}
	armed_ = false;
	report.trace = trace_;
	report.uartRx = current.uartRx - begin_.uartRx;
	report.uartTx = current.uartTx - begin_.uartTx;
	report.rxFull = current.rxFull - begin_.rxFull;
	report.rxOverrun = current.rxOverrun - begin_.rxOverrun;
	report.rxFraming = current.rxFraming - begin_.rxFraming;
	report.acknowledged = current.acknowledged - begin_.acknowledged;
	report.accepted = current.accepted - begin_.accepted;
	report.retries = current.retries - begin_.retries;
	report.blocked = current.blocked - begin_.blocked;
	report.bad = current.bad - begin_.bad;
	report.hwArc = current.hwArc - begin_.hwArc;
	report.maxRt = current.maxRt - begin_.maxRt;
	report.ackFifoFull = current.ackFifoFull - begin_.ackFifoFull;
	report.rxFifoFull = current.rxFifoFull - begin_.rxFifoFull;
	report.sid = current.sid;
	report.outstanding = current.outstanding;
	report.credit = current.credit;
	report.freeInput = current.freeInput;
	return true;
}

uint16_t PpUartQuietDiagnostic::milliseconds(uint32_t ticks) {
	const uint32_t ms = ticks / PP_UART_DIAG_TICKS_PER_MS;
	return static_cast<uint16_t>(ms > UINT16_MAX ? UINT16_MAX : ms);
}

void PpUartQuietDiagnostic::record(
	PpUartDiagEventKind kind, uint32_t now, const PpUartDiagSnapshot& current) {
	const uint8_t index = static_cast<uint8_t>(kind);
	const uint8_t bit = 1U << index;
	if ((trace_.seen & bit) != 0) {
		return;
	}
	trace_.seen = trace_.seen | bit;
	PpUartDiagEvent& event = trace_.events[index];
	event.atMs = milliseconds((now - started_));
	event.maxRt = current.maxRt - begin_.maxRt;
	event.retries = current.retries - begin_.retries;
	event.bad = current.bad - begin_.bad;
	event.rxFull = current.rxFull - begin_.rxFull;
	event.outstanding = current.outstanding;
	event.credit = current.credit;
	event.freeInput = current.freeInput;
}

void PpUartQuietDiagnostic::observeTrace(uint32_t now, const PpUartDiagSnapshot& current) {
	if (current.freeInput < trace_.minFree) {
		trace_.minFree = current.freeInput;
	}
	// A completed ACK interval must be measured BEFORE resetting its
	// origin; otherwise a stall ending between samples disappears.
	if (hadBacklog_) {
		const uint16_t elapsed = milliseconds((now - ackProgressAt_));
		if (elapsed > trace_.maxAckPauseMs) {
			trace_.maxAckPauseMs = elapsed;
		}
	}
	const bool backlog =
		current.outstanding != 0 || current.freeInput < PP_UART_DIAG_EMPTY_INPUT_FREE;
	if (!hadBacklog_ || !backlog || current.acknowledged != previous_.acknowledged) {
		ackProgressAt_ = now;
	}
	hadBacklog_ = backlog;
	if (current.maxRt != previous_.maxRt) {
		record(PpUartDiagEventKind::MAX_RT, now, current);
	}
	if (current.retries != previous_.retries) {
		record(PpUartDiagEventKind::REPLAY, now, current);
	}
	if (current.bad != previous_.bad) {
		record(PpUartDiagEventKind::GAP, now, current);
	}
	if (current.rxFull != previous_.rxFull) {
		record(PpUartDiagEventKind::RX_FULL, now, current);
	}
}

bool formatPpUartDiagTrace(
	PpStressLine& line, bool leader, const PpUartDiagTrace& trace, uint8_t index) {
	if (index > PP_UART_DIAG_EVENT_COUNT) {
		return false;
	}
	if (index == 0) {
		line.word(leader ? "A wait minfree=" : "B wait minfree=");
		line.decimal(trace.minFree);
		line.word(" ackpause_ms=");
		line.decimal(trace.maxAckPauseMs);
	} else {
		const uint8_t eventIndex = index - 1U;
		if ((trace.seen & static_cast<uint8_t>(1U << eventIndex)) == 0) {
			return false;
		}
		const PpUartDiagEvent& event = trace.events[eventIndex];
		line.word(leader ? "A event k=" : "B event k=");
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
