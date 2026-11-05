#!/usr/bin/env sh
# Compile every active bridge/benchmark variant for ATmega328P.
# Use one compiler recipe so the diagnostic build cannot silently diverge.
set -eu
cd "$(dirname "$0")/.."
AVRCXX="${AVRCXX:-avr-g++}"
AVRSIZE="${AVRSIZE:-avr-size}"
OUTPUT="${TMPDIR:-/tmp}/gpt-nrf24-avr-$$"
mkdir -p "$OUTPUT"
trap 'rm -rf "$OUTPUT"' EXIT HUP INT TERM

build_variant() {
    label="$1"
    mode="$2"
    diagnostic="$3"
    "$AVRCXX" -std=c++11 -Os -mmcu=atmega328p -DF_CPU=16000000UL \
        "-DPP_TEST_TRAFFIC_MODE=$mode" \
        "-DPP_UART_DIAG_ENABLED=$diagnostic" \
        -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections \
        -Wl,--gc-sections -Wall -Wextra \
        GPT_NRF2401/*.cpp -o "$OUTPUT/$label.elf"
    "$AVRSIZE" "$OUTPUT/$label.elf"
    printf 'PASS AVR mode %s\n' "$label"
}

build_variant PP_UART_BRIDGE_DIAGNOSTIC PP_TRAFFIC_MODE_UART_BRIDGE 1
build_variant PP_UART_BRIDGE_CLEAN PP_TRAFFIC_MODE_UART_BRIDGE 0
build_variant PP_SYNTHETIC_RADIO PP_TRAFFIC_MODE_SYNTHETIC_RADIO 0
