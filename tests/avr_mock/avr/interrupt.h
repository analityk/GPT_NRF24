#ifndef GPT_TEST_AVR_INTERRUPT_H
#define GPT_TEST_AVR_INTERRUPT_H
#define ISR(vector) void vector()
void USART_RX_vect();
void USART_UDRE_vect();
#endif
