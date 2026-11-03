#!/usr/bin/env sh
# Discover and test every portable C++11 regression in this repository.
# Firmware-specific code is compiled separately with the AVR toolchain.
set -eu
cd "$(dirname "$0")/.."
CXX="${CXX:-g++}"
OUT="${TMPDIR:-/tmp}/gpt-nrf24-host-$$"
mkdir -p "$OUT"
trap 'rm -rf "$OUT"' EXIT HUP INT TERM
for source in tests/*_test.cpp; do
    [ -f "$source" ] || { echo 'No host tests found' >&2; exit 1; }
    filename=${source##*/}
    name=${filename%_test.cpp}
    "$CXX" -std=c++11 -Wall -Wextra -Werror -pedantic \
        "$source" -o "$OUT/$name"
    "$OUT/$name"
    printf 'PASS %s\n' "$name"
done
