/*
 * GPT_NRF2401.cpp
 *
 * Created: 02.11.2026 21:51:53
 * Author : Pc
 */ 

#include <avr/io.h>
#include "gpio.h"

void delay(uint32_t volatile t){
	while(t--){};
}


int main(void)
{
	GPIO led1(&PORTD, PIND0);
    /* Replace with your application code */
    while (1) 
    {
		delay(0xFFFF);
		led1.flip();
    }
}

