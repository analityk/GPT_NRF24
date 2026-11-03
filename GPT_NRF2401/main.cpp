/*
 * UART bring-up: transparent, nonblocking local echo on USB/CH340.
 * Each board runs the same binary. USART0 owns PD0/RXD and PD1/TXD.
 * Previous GPIO LED test remains in Git history (commit 9d251e1).
 */
#include <avr/interrupt.h>
#include "uart.h"

// Explicit singleton: buffers live in static SRAM, never on the call stack or heap.
static UART uart;

int main(void) {
    uart.begin();
    sei();

    for (;;) {
        // Do not consume RX unless there is TX queue space.
        // Dropped RX bytes are counted by the driver if the producer outruns us.
        if (uart.freeSpace() != 0) {
            uint8_t byte;
            if (uart.read(byte)) {
                (void)uart.write(byte);
            }
        }
    }
}
