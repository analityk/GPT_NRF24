#ifndef GPT_NRF24_PP_TEST_TRAFFIC_MODE_H
#define GPT_NRF24_PP_TEST_TRAFFIC_MODE_H

// Select a single data source/sink for both boards at compile time.
// Only the synthetic radio benchmark may print diagnostic text to USART0.
#define PP_TRAFFIC_MODE_SYNTHETIC_RADIO 0U
#define PP_TRAFFIC_MODE_UART_BRIDGE 1U

#ifndef PP_TEST_TRAFFIC_MODE
#define PP_TEST_TRAFFIC_MODE PP_TRAFFIC_MODE_UART_BRIDGE
#endif

#if PP_TEST_TRAFFIC_MODE != PP_TRAFFIC_MODE_SYNTHETIC_RADIO && \
	PP_TEST_TRAFFIC_MODE != PP_TRAFFIC_MODE_UART_BRIDGE
#error "Unsupported PP_TEST_TRAFFIC_MODE"
#endif


// The bridge diagnostic is temporary and intentionally appends ASCII
// reports only after a quiet interval. Disable for byte-exact captures.
#ifndef PP_UART_DIAG_ENABLED
#if PP_TEST_TRAFFIC_MODE == PP_TRAFFIC_MODE_UART_BRIDGE
#define PP_UART_DIAG_ENABLED 1
#else
#define PP_UART_DIAG_ENABLED 0
#endif
#endif

#endif
