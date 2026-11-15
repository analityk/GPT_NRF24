#ifndef GPT_TEST_AVR_IO_H
#define GPT_TEST_AVR_IO_H
#include <stdint.h>
#define _BV(bit) (1U << (bit))
enum { U2X0=1, UPE0=2, DOR0=3, FE0=4, UDRIE0=5, RXCIE0=7,
       RXEN0=4, TXEN0=3, UCSZ00=1, UCSZ01=2 };
extern volatile uint8_t UCSR0A;
extern volatile uint8_t UCSR0B;
extern volatile uint8_t UCSR0C;
extern volatile uint8_t UBRR0H;
extern volatile uint8_t UBRR0L;
extern volatile uint8_t UDR0;
#endif
