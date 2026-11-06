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
    shift
    "$AVRCXX" -std=c++11 -Os -mmcu=atmega328p -DF_CPU=16000000UL \
        "$@" \
        -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections \
        -Wl,--gc-sections -Wall -Wextra \
        GPT_NRF2401/*.cpp -o "$OUTPUT/$label.elf"
    "$AVRSIZE" "$OUTPUT/$label.elf"
    printf 'PASS AVR mode %s\n' "$label"
}

build_variant PP_DEFAULT
build_variant PP_UART_BRIDGE_DIAGNOSTIC -DPP_TEST_TRAFFIC_MODE=PP_TRAFFIC_MODE_UART_BRIDGE -DPP_UART_DIAG_ENABLED=1
build_variant PP_UART_BRIDGE_CLEAN -DPP_TEST_TRAFFIC_MODE=PP_TRAFFIC_MODE_UART_BRIDGE -DPP_UART_DIAG_ENABLED=0
build_variant PP_SYNTHETIC_RADIO -DPP_TEST_TRAFFIC_MODE=PP_TRAFFIC_MODE_SYNTHETIC_RADIO -DPP_UART_DIAG_ENABLED=0
