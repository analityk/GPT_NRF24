#ifndef GPT_NRF24_APPLICATION_SELECT_H
#define GPT_NRF24_APPLICATION_SELECT_H

// One Microchip Studio project, three independent firmware applications:
// 0 = previously verified UART bridge (legacy v1)
// 1 = previously verified raw full 32B ACK Payload benchmark
// 2 = paired diagnostic core, 8B header + 0..24B opaque data (experimental)
// Manual F7 build / F12 program unchanged; NO Post-Build flash.
#define GPT_NRF24_APPLICATION 2

#endif
