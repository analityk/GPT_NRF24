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

#endif
