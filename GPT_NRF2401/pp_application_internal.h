#ifndef GPT_PP_APPLICATION_INTERNAL_H
#define GPT_PP_APPLICATION_INTERNAL_H
#include "board_clock.h"
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "gpio.h"
#include "uart.h"
#include "spi.h"
#include "nrf24.h"
#include "avr_delay.h"
#include "adc0.h"
#include "timer1_clock.h"
#include "paired_frame.h"
#include "paired_link.h"
#include "paired_session.h"
#include "uart_reliable_stream.h"
#include "pp_echo_ramp_port.h"
#include "pp_uart_bridge_port.h"
#include "pp_test_traffic_mode.h"
#include "pp_stress_telemetry.h"
#include "pp_cycle_timing.h"
#include "pp_short_probe.h"
#include "pp_uart_quiet_diagnostic.h"
#include "boot_epoch.h"
#include "nrf24_ack_phy.h"
#include "pp_reply_mailbox.h"
#include "pp_recovery_control.h"
#include "host_flow_status.h"

#define PP_PTX_PEER_STARTUP_MS 150U
#define PP_RECOVERY_TURN_TICKS 3200UL
#define PP_RECOVERY_WARM_TIMEOUT_TICKS 2000000UL
#define PP_PRX_HANDSHAKE_TIMEOUT_TICKS 4000000UL
#define PP_TEST_RADIO_STARTUP_MS 110U
#define PP_TEST_ACK_RETRY_SETUP 0x23U
#define PP_TEST_RECOVERY_REPORT_INITIAL_OFFSET_TICKS 400000UL
#define PP_OFFLINE_PROBE_INTERVAL_TICKS 40000UL
#define PP_OFFLINE_IDLE_US 100U
#define PP_PTX_HANDSHAKE_TIMEOUT_TICKS 2000000UL
#define PP_PTX_BACKGROUND_MAX_GENERATED_BYTES PairedFrame::MAX_BODY
#define PP_TEST_RF_SETUP_VALUE NRF24_RF_SETUP_2_MBPS
#define PP_PTX_TIMING_PROBE_ENABLED PP_PHY_MEASURE_TIMING
#if PP_TEST_TRAFFIC_MODE == PP_TRAFFIC_MODE_UART_BRIDGE
#define PP_SOURCE_PROBE_ENABLED 0
#else
#define PP_SOURCE_PROBE_ENABLED 1
#endif
#define PP_SOURCE_PROBE_REPORT_INTERVAL_TICKS 2000000UL
#define PP_SOURCE_PROBE_REPORT_MIN_FREE_BYTES 80U
#define PP_TIMING_REPORT_INTERVAL_TICKS 2000000UL
#define PP_TIMING_REPORT_PHASE_OFFSET_TICKS 1000000UL
#define PP_TIMING_REPORT_MIN_FREE_BYTES 120U

namespace PpApp {
typedef Nrf24 Radio;
typedef NrfAckPayloadPhy AckPhy;
#if PP_TEST_TRAFFIC_MODE == PP_TRAFFIC_MODE_UART_BRIDGE
typedef PpUartBridgePort ActivePort;
#else
typedef PpEchoRampPort ActivePort;
#endif
extern UART uart;
extern ActivePort diagnosticPort;
extern SpiMaster spi;
extern GPIO ce;
extern GPIO csn;
extern Radio radio;
extern AvrDelay delay;
extern Timer1Clock ticks;
extern AckPhy phy;
extern PairedLink linkState;
extern PairedSession session;
extern UartReliableStream stream;
extern bool isLeader;
extern uint32_t lastPeerProgress;
extern PpRecoveryControl recovery;
extern uint32_t lastProbe;
#if PP_TEST_TRAFFIC_MODE == PP_TRAFFIC_MODE_SYNTHETIC_RADIO
extern PpRecoveryControl::State lastPrintedState;
extern bool lastPrintedUncertainty;
PpStressSnapshot performanceSnapshot();
#endif
extern uint32_t lastRecoveryReport;
extern uint16_t recoveryCount;
extern uint16_t radioErrors;
extern bool replaceAck;
extern uint8_t packet[PairedFrame::MAX_FRAME];
extern uint8_t incoming[PairedFrame::MAX_FRAME];
extern uint8_t ackStaged[PairedFrame::MAX_FRAME];
extern uint8_t ackSize;
extern bool ackWaiting;
extern PpStressWindow performance;
extern PpCycleTimingWindow cycleTiming;
extern PpShortProbeWindow sourceProbe;
extern uint32_t lastSourceProbeReport;
extern PpCycleTimingWindow::Average pendingTimingDetail;
extern bool timingDetailPending;
extern uint32_t hardwareRetries;
extern uint32_t maxRetries;
extern uint32_t lastReport;
extern uint32_t lastTimingReport;
extern uint16_t ackFifoFull;
#if PP_UART_DIAG_ENABLED
extern bool previousRfDataExchange;
void markRfCycle(uint16_t started, bool successful);
#endif
void serviceUartDiagnostic();
void reportRecoveryTransition();
void reportPerformance();
void reportCycleTiming();
void reportSourceProbe(uint32_t now);
void fatal();
void runA();
void runB();
} // namespace PpApp
#endif
