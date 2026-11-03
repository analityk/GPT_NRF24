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
"$AVRCXX" -std=c++11 -Os -mmcu=atmega328p -DF_CPU=16000000UL \
    -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections \
    -Wl,--gc-sections -Wall -Wextra \
    GPT_NRF2401/*.cpp -o "$OUTPUT/paired-core.elf"
"$AVRSIZE" "$OUTPUT/paired-core.elf"
printf 'PASS AVR paired core\n'
