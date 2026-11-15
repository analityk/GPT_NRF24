#!/usr/bin/env sh
# Discover and test every portable C++11 regression in this repository.
# Firmware-specific code is compiled separately with the AVR toolchain.
set -eu
cd "$(dirname "$0")/.."
CXX="${CXX:-g++}"
python3 tests/check_source_contract.py
OUT="${TMPDIR:-/tmp}/gpt-nrf24-host-$$"
mkdir -p "$OUT"
trap 'rm -rf "$OUT"' EXIT HUP INT TERM
for source in tests/*_test.cpp; do
    [ -f "$source" ] || { echo 'No host tests found' >&2; exit 1; }
    filename=${source##*/}
    name=${filename%_test.cpp}
    extra=""
    for module in GPT_NRF2401/*.cpp; do
        case "$module" in
            */pp_application.cpp|*/pp_runner.cpp|*/pp_console_diagnostics.cpp|*/main.cpp|*/uart.cpp|*/spi.cpp|*/gpio.cpp|*/adc0.cpp|*/timer1_clock.cpp|*/boot_epoch.cpp|*/avr_delay.cpp) ;;
            *) extra="$extra $module" ;;
        esac
    done
    flags="-DGPT_HOST_TEST"
    if [ "$name" = spi_block_transfer ]; then
        extra="$extra GPT_NRF2401/spi.cpp"
        flags="-DGPT_HOST_TEST"
    fi
    if [ "$name" = uart_isr ]; then
        extra="$extra GPT_NRF2401/uart.cpp"
        flags="$flags -Itests/avr_mock"
    fi
    if [ "$name" = pp_async_phy ]; then
        flags="-DGPT_HOST_TEST -DPP_PHY_MEASURE_TIMING=1"
    fi
    "$CXX" $flags ${HOST_CXXFLAGS:-} -std=c++11 -Wall -Wextra -Werror -pedantic \
        "$source" $extra -o "$OUT/$name"
    "$OUT/$name"
    printf 'PASS %s\n' "$name"
done
