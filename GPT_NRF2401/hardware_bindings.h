#ifndef GPT_HARDWARE_BINDINGS_H
#define GPT_HARDWARE_BINDINGS_H
// Firmware binds directly to its hardware drivers. Host tests replace only
// this boundary with explicit function tables; the protocol code is shared.
#ifdef GPT_HOST_TEST
#include "foreground_ports.h"
typedef BytePort StreamHandle;
typedef BytePort ConsoleHandle;
typedef SpiPort SpiHandle;
typedef PinPort PinHandle;
typedef RadioPort RadioHandle;
typedef ClockPort ClockHandle;
typedef DelayPort DelayHandle;
#else
#include "pp_test_traffic_mode.h"
class UART;
class PpUartBridgePort;
class PpEchoRampPort;
typedef UART& ConsoleHandle;
#if PP_TEST_TRAFFIC_MODE == PP_TRAFFIC_MODE_UART_BRIDGE
typedef PpUartBridgePort& StreamHandle;
#else
typedef PpEchoRampPort& StreamHandle;
#endif
class SpiMaster;
class GPIO;
class Nrf24;
class Timer1Clock;
class AvrDelay;
typedef SpiMaster& SpiHandle;
typedef GPIO& PinHandle;
typedef Nrf24& RadioHandle;
typedef Timer1Clock& ClockHandle;
typedef AvrDelay& DelayHandle;
#endif
#endif
