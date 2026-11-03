#!/usr/bin/env sh
# Portable C++11 host-only protocol regressions. Does not build AVR firmware.
set -eu
cd "$(dirname "$0")/.."
CXX="${CXX:-g++}"
OUT="${TMPDIR:-/tmp}/gpt-nrf24-host-$$"
mkdir -p "$OUT"
trap 'rm -rf "$OUT"' EXIT HUP INT TERM
for name in uart_ring nrf24_mock rf_raw_pattern nrf24_phy frame_v1 v1_probe_pattern session_v1 uart_transport_v1 uart_transport_stress ack_bench_pattern nrf24_ack_phy; do
    "$CXX" -std=c++11 -Wall -Wextra -Werror -pedantic \
        "tests/${name}_test.cpp" -o "$OUT/$name"
    "$OUT/$name"
    printf 'PASS %s\n' "$name"
done
