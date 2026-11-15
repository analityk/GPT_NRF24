#ifndef GPT_NRF24_PP_UART_QUIET_DIAGNOSTIC_H
#define GPT_NRF24_PP_UART_QUIET_DIAGNOSTIC_H

#include <stdint.h>
#include "pp_stress_telemetry.h"

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
enum class PpUartDiagEventKind : uint8_t { MAX_RT, REPLAY, GAP, RX_FULL };

struct PpUartDiagEvent {
	uint16_t atMs, maxRt, retries, bad, rxFull;
	uint8_t outstanding, credit, freeInput;
};

struct PpUartDiagTrace {
	PpUartDiagEvent events[PP_UART_DIAG_EVENT_COUNT];
	uint8_t seen, minFree;
	uint16_t maxAckPauseMs;
	PpUartDiagTrace();
};

// Reuse exactly this formatter in firmware and host length tests.
// Index zero is the summary; the remaining indices select event kinds.
bool formatPpUartDiagTrace(
	PpStressLine& line, bool leader, const PpUartDiagTrace& trace, uint8_t index);

struct PpUartDiagSnapshot {
	uint16_t uartRx, uartTx, rxFull, rxOverrun, rxFraming;
	uint16_t acknowledged, accepted, retries, blocked, bad;
	uint16_t hwArc, maxRt, ackFifoFull, rxFifoFull;
	uint16_t sid;
	uint8_t outstanding, credit, freeInput;
	PpUartDiagSnapshot();
};

struct PpUartDiagReport {
	PpUartDiagTrace trace;
	uint16_t uartRx, uartTx, rxFull, rxOverrun, rxFraming;
	uint16_t acknowledged, accepted, retries, blocked, bad;
	uint16_t hwArc, maxRt, ackFifoFull, rxFifoFull;
	uint16_t sid;
	uint8_t outstanding, credit, freeInput;
	PpUartDiagReport();
};

// Reusable quiet-interval detector. It tracks bytes arriving at the UART,
// their loss at RX, data committed to the peer's UART, and application ACKs.
// Radio retries alone do not postpone a report for a stalled transport.
class PpUartQuietDiagnostic {
  public:
	PpUartQuietDiagnostic();

	bool observe(uint32_t now, const PpUartDiagSnapshot& current, PpUartDiagReport& report);

  private:
	static uint16_t milliseconds(uint32_t ticks);

	void record(PpUartDiagEventKind kind, uint32_t now, const PpUartDiagSnapshot& current);

	void observeTrace(uint32_t now, const PpUartDiagSnapshot& current);

	PpUartDiagSnapshot begin_, previous_;
	PpUartDiagTrace trace_;
	bool initialized_, armed_;
	uint32_t lastActive_;
	uint32_t started_, ackProgressAt_;
	bool hadBacklog_;
};

#endif
