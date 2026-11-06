#ifndef GPT_NRF24_PP_TEST_TRAFFIC_MODE_H
#define GPT_NRF24_PP_TEST_TRAFFIC_MODE_H

// Select a single data source/sink for both boards at compile time.
// Synthetic prints a live benchmark; the bridge diagnostic (when enabled)
// prints only after a quiet interval, never inside an active data burst.
#define PP_TRAFFIC_MODE_SYNTHETIC_RADIO 0U
#define PP_TRAFFIC_MODE_UART_BRIDGE 1U

#ifndef PP_TEST_TRAFFIC_MODE
#define PP_TEST_TRAFFIC_MODE PP_TRAFFIC_MODE_UART_BRIDGE
#endif

#if PP_TEST_TRAFFIC_MODE != PP_TRAFFIC_MODE_SYNTHETIC_RADIO && \
	PP_TEST_TRAFFIC_MODE != PP_TRAFFIC_MODE_UART_BRIDGE
#error "Unsupported PP_TEST_TRAFFIC_MODE"
#endif


// UART is a transparent data stream by default, including after idle.
// Explicitly opt in to ASCII reports only for a diagnostic firmware build.
#ifndef PP_UART_DIAG_ENABLED
#define PP_UART_DIAG_ENABLED 0
#endif

#endif
