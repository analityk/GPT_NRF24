#!/usr/bin/env sh
# Cross-compile all three Microchip Studio application profiles without
# flashing hardware or modifying user project settings.
set -eu
cd "$(dirname "$0")/.."
AVRCXX="${AVRCXX:-avr-g++}"
AVRSIZE="${AVRSIZE:-avr-size}"
OUTPUT="${TMPDIR:-/tmp}/gpt-nrf24-avr-$$"
mkdir -p "$OUTPUT"
trap 'rm -rf "$OUTPUT"' EXIT HUP INT TERM
for profile in 0 1 2; do
    "$AVRCXX" -std=c++11 -Os -mmcu=atmega328p -DF_CPU=16000000UL \
        "-DGPT_NRF24_APPLICATION=$profile" \
        -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections \
        -Wl,--gc-sections -Wall -Wextra \
        GPT_NRF2401/*.cpp -o "$OUTPUT/firmware-$profile.elf"
    "$AVRSIZE" "$OUTPUT/firmware-$profile.elf"
    printf 'PASS avr profile %s\n' "$profile"
done
