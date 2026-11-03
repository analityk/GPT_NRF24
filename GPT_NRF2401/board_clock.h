#ifndef GPT_NRF24_BOARD_CLOCK_H
#define GPT_NRF24_BOARD_CLOCK_H

// Single source of truth for Microchip Studio / AVR-GCC clock assumptions.
// Include before util/delay.h in every translation unit using time delays.
#ifndef F_CPU
#define F_CPU 16000000UL
#endif
#if F_CPU != 16000000UL
#error "Current RF-Nano firmware requires F_CPU=16000000UL"
#endif
#endif
