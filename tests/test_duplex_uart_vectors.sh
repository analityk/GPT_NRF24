#!/usr/bin/env sh
# Validate exact two-direction capture comparison, including corruption.
set -eu
cd "$(dirname "$0")/.."
CASE_DIR="$(mktemp -d)"
trap 'rm -rf "$CASE_DIR"' EXIT HUP INT TERM
VECTOR_BYTES=1024
python3 tools/duplex_uart_vectors.py create --directory "$CASE_DIR" --size "$VECTOR_BYTES"
cp "$CASE_DIR/a_to_b.bin" "$CASE_DIR/capture_at_b.bin"
cp "$CASE_DIR/b_to_a.bin" "$CASE_DIR/capture_at_a.bin"
python3 tools/duplex_uart_vectors.py verify --directory "$CASE_DIR"
printf '!' >> "$CASE_DIR/capture_at_a.bin"
if python3 tools/duplex_uart_vectors.py verify --directory "$CASE_DIR"; then
    echo 'FAIL: corrupted full-duplex capture was accepted' >&2
    exit 1
fi
echo 'PASS duplex UART vector generator and verifier'
