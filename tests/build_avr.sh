#!/usr/bin/env sh
# Compile the active paired core for ATmega328P without flashing hardware.
# All source files in the firmware directory belong to the one active app.
set -eu
cd "$(dirname "$0")/.."
AVRCXX="${AVRCXX:-avr-g++}"
AVRSIZE="${AVRSIZE:-avr-size}"
OUTPUT="${TMPDIR:-/tmp}/gpt-nrf24-avr-$$"
mkdir -p "$OUTPUT"
trap 'rm -rf "$OUTPUT"' EXIT HUP INT TERM
# Both traffic modes share exactly the same PHY and protocol core.
# Compile both variants to keep the reversible benchmark working.
for mode in PP_TRAFFIC_MODE_UART_BRIDGE PP_TRAFFIC_MODE_SYNTHETIC_RADIO; do
    "$AVRCXX" -std=c++11 -Os -mmcu=atmega328p -DF_CPU=16000000UL \
        "-DPP_TEST_TRAFFIC_MODE=$mode" \
        -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections \
        -Wl,--gc-sections -Wall -Wextra \
        GPT_NRF2401/*.cpp -o "$OUTPUT/$mode.elf"
    "$AVRSIZE" "$OUTPUT/$mode.elf"
    printf 'PASS AVR mode %s\n' "$mode"
done
