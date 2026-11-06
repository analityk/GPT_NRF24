#include "../GPT_NRF2401/pp_uart_quiet_diagnostic.h"
#include "../GPT_NRF2401/pp_short_probe.h"
#include "../GPT_NRF2401/pp_stress_telemetry.h"
#include <cassert>

#define DIAG_TEST_START_TICKS 4000000UL
#define DIAG_TEST_INTERVAL_TICKS 20000UL
#define DIAG_TEST_SAMPLE_QUIET_TICKS PP_UART_DIAG_QUIET_TICKS
#define DIAG_TEST_SAMPLE_RX_BYTES 128U
#define DIAG_TEST_SAMPLE_ACK_BYTES 96U
#define DIAG_TEST_SAMPLE_ACCEPTED_BYTES 72U
#define DIAG_TEST_SAMPLE_OVERFLOW_BYTES 7U
#define DIAG_TEST_SAMPLE_RETRIES 4U
#define DIAG_TEST_SAMPLE_BLOCKED 2U
#define DIAG_TEST_SAMPLE_HW_ARC 9U
#define DIAG_TEST_SAMPLE_MAX_RT 1U
#define DIAG_TEST_SAMPLE_ACK_FULL 13U
#define DIAG_TEST_SAMPLE_RX_FIFO_FULL 3U
#define DIAG_TEST_SAMPLE_SID 0x5678U
#define DIAG_TEST_SAMPLE_FREE 79U
#define DIAG_TEST_SAMPLE_CREDIT 3U
#define DIAG_TEST_REPEATED_LINES 100U
#define DIAG_TEST_BYTES_PER_LINE 128U
#define DIAG_TEST_LINE_GAP_TICKS 20000UL
#define DIAG_TEST_WRAP_FIRST 65530U
#define DIAG_TEST_WRAP_SECOND 6U
#define DIAG_TEST_WRAP_DELTA 12U

static void testQuietGapAndProgress(){
	PpUartQuietDiagnostic watcher;
	PpUartDiagSnapshot s;
	PpUartDiagReport report;
	uint32_t now=DIAG_TEST_START_TICKS;
	assert(!watcher.observe(now,s,report));
	s.uartRx=DIAG_TEST_SAMPLE_RX_BYTES;
	s.acknowledged=DIAG_TEST_SAMPLE_ACK_BYTES;
	s.accepted=DIAG_TEST_SAMPLE_ACCEPTED_BYTES;
	s.rxFull=DIAG_TEST_SAMPLE_OVERFLOW_BYTES;
	s.retries=DIAG_TEST_SAMPLE_RETRIES;
	s.blocked=DIAG_TEST_SAMPLE_BLOCKED;
	s.hwArc=DIAG_TEST_SAMPLE_HW_ARC;
	s.maxRt=DIAG_TEST_SAMPLE_MAX_RT;
	s.ackFifoFull=DIAG_TEST_SAMPLE_ACK_FULL;
	s.rxFifoFull=DIAG_TEST_SAMPLE_RX_FIFO_FULL;
	s.sid=DIAG_TEST_SAMPLE_SID;
	s.freeInput=DIAG_TEST_SAMPLE_FREE;
	s.credit=DIAG_TEST_SAMPLE_CREDIT;
	now+=DIAG_TEST_INTERVAL_TICKS;
	assert(!watcher.observe(now,s,report));
	// A 10ms CR/LF gap must never emit telemetry into the next UART burst.
	now+=DIAG_TEST_INTERVAL_TICKS;
	assert(!watcher.observe(now,s,report));
	// The interval is counted since the LAST progress, not the start.
	now+=DIAG_TEST_SAMPLE_QUIET_TICKS-DIAG_TEST_INTERVAL_TICKS;
	assert(watcher.observe(now,s,report));
	assert(report.uartRx==DIAG_TEST_SAMPLE_RX_BYTES);
	assert(report.acknowledged==DIAG_TEST_SAMPLE_ACK_BYTES);
	assert(report.accepted==DIAG_TEST_SAMPLE_ACCEPTED_BYTES);
	assert(report.rxFull==DIAG_TEST_SAMPLE_OVERFLOW_BYTES);
	assert(report.retries==DIAG_TEST_SAMPLE_RETRIES);
	assert(report.blocked==DIAG_TEST_SAMPLE_BLOCKED);
	assert(report.hwArc==DIAG_TEST_SAMPLE_HW_ARC);
	assert(report.maxRt==DIAG_TEST_SAMPLE_MAX_RT);
	assert(report.ackFifoFull==DIAG_TEST_SAMPLE_ACK_FULL);
	assert(report.rxFifoFull==DIAG_TEST_SAMPLE_RX_FIFO_FULL);
	assert(report.sid==DIAG_TEST_SAMPLE_SID);
	assert(report.freeInput==DIAG_TEST_SAMPLE_FREE);
	assert(report.credit==DIAG_TEST_SAMPLE_CREDIT);
	assert(!watcher.observe(now+DIAG_TEST_SAMPLE_QUIET_TICKS,s,report));
	s.accepted=static_cast<uint16_t>(s.accepted+1U);
	now+=DIAG_TEST_SAMPLE_QUIET_TICKS;
	assert(!watcher.observe(now,s,report));
	now+=DIAG_TEST_SAMPLE_QUIET_TICKS;
	assert(watcher.observe(now,s,report));
	assert(report.accepted==1U);
	assert(report.uartRx==0U);
}

static void testRepeatedLineGaps(){
	PpUartQuietDiagnostic watcher;
	PpUartDiagSnapshot sample;
	PpUartDiagReport report;
	uint32_t now=DIAG_TEST_START_TICKS;
	assert(!watcher.observe(now,sample,report));
	for(uint16_t line=0;line<DIAG_TEST_REPEATED_LINES;++line){
		sample.uartRx=static_cast<uint16_t>(
			sample.uartRx+DIAG_TEST_BYTES_PER_LINE);
		now+=DIAG_TEST_LINE_GAP_TICKS;
		assert(!watcher.observe(now,sample,report));
	}
	now+=DIAG_TEST_SAMPLE_QUIET_TICKS;
	assert(watcher.observe(now,sample,report));
	assert(report.uartRx==
		DIAG_TEST_REPEATED_LINES*DIAG_TEST_BYTES_PER_LINE);
}

static void testWrapAndHardwareOverflow(){
	PpUartQuietDiagnostic watcher;
	PpUartDiagSnapshot s;
	PpUartDiagReport report;
	uint32_t now=DIAG_TEST_START_TICKS;
	s.uartRx=DIAG_TEST_WRAP_FIRST;
	s.rxOverrun=DIAG_TEST_WRAP_FIRST;
	s.hwArc=DIAG_TEST_WRAP_FIRST;
	s.ackFifoFull=DIAG_TEST_WRAP_FIRST;
	assert(!watcher.observe(now,s,report));
	s.uartRx=DIAG_TEST_WRAP_SECOND;
	s.rxOverrun=DIAG_TEST_WRAP_SECOND;
	s.hwArc=DIAG_TEST_WRAP_SECOND;
	s.ackFifoFull=DIAG_TEST_WRAP_SECOND;
	now+=DIAG_TEST_INTERVAL_TICKS;
	assert(!watcher.observe(now,s,report));
	now+=DIAG_TEST_SAMPLE_QUIET_TICKS;
	assert(watcher.observe(now,s,report));
	assert(report.uartRx==DIAG_TEST_WRAP_DELTA);
	assert(report.rxOverrun==DIAG_TEST_WRAP_DELTA);
	assert(report.hwArc==DIAG_TEST_WRAP_DELTA);
	assert(report.ackFifoFull==DIAG_TEST_WRAP_DELTA);
}

static void testLineLength(){
	// The standard line builder exposes exact size for excluding its
	// diagnostic UART characters from the physical TX byte counter.
	PpStressLine line;
	line.word("A rf n=");
	line.decimal(DIAG_TEST_SAMPLE_ACK_BYTES);
	line.word(" cyc=");
	line.decimal(DIAG_TEST_SAMPLE_RX_BYTES);
	assert(line.length()!=0);
	assert(line.length()<=PP_UART_DIAG_LINE_FREE_BYTES);
	// Worst-case 16-bit decimal values from the actual A/B PP report.
	PpStressLine status;
	status.word("A pp ack=");status.decimal(UINT16_MAX);
	status.word(" rx=");status.decimal(UINT16_MAX);
	status.word(" ret=");status.decimal(UINT16_MAX);
	status.word(" blk=");status.decimal(UINT16_MAX);
	status.word(" bad=");status.decimal(UINT16_MAX);
	status.word(" win=");status.decimal(UINT8_MAX);
	status.word(" cr=");status.decimal(UINT8_MAX);
	status.word(" hw=");status.decimal(UINT16_MAX);
	status.word(" mx=");status.decimal(UINT16_MAX);
	status.word(" sid=");status.decimal(UINT16_MAX);
	status.word("\r\n");
	assert(status.length()<=PP_UART_DIAG_LINE_FREE_BYTES);
}

int main(){
	testQuietGapAndProgress();
	testRepeatedLineGaps();
	testWrapAndHardwareOverflow();
	testLineLength();
}
