#!/usr/bin/env python3
"""Deterministic full-duplex UART test vectors and exact capture verifier."""

import argparse
import hashlib
from pathlib import Path

VECTOR_A = "a_to_b.bin"
VECTOR_B = "b_to_a.bin"
CAPTURE_A = "capture_at_a.bin"
CAPTURE_B = "capture_at_b.bin"
DEFAULT_SIZE = 8192
FIRST_BYTES_A = bytes((0x00, ord("s"), ord("w"), 0xFF, 0x0D, 0x0A))
FIRST_BYTES_B = bytes((0xFF, ord("w"), ord("s"), 0x00, 0x0A, 0x0D))
A_MULTIPLIER = 37
B_MULTIPLIER = 53
A_SALT = 11
B_SALT = 79
BYTE_MASK = 0xFF
POSITION_SHIFT = 3


def pattern(length: int, multiplier: int, salt: int, prefix: bytes) -> bytes:
    if length < 0:
        raise ValueError("Length cannot be negative")
    data = bytearray(
        (index * multiplier + (index >> POSITION_SHIFT) + salt) & BYTE_MASK
        for index in range(length)
    )
    data[: min(length, len(prefix))] = prefix[:length]
    return bytes(data)


def create(directory: Path, length: int) -> None:
    directory.mkdir(parents=True, exist_ok=True)
    values = (
        (VECTOR_A, A_MULTIPLIER, A_SALT, FIRST_BYTES_A),
        (VECTOR_B, B_MULTIPLIER, B_SALT, FIRST_BYTES_B),
    )
    for filename, multiplier, salt, prefix in values:
        path = directory / filename
        payload = pattern(length, multiplier, salt, prefix)
        path.write_bytes(payload)
        print(f"{path}: {len(payload)} bytes, SHA256 {hashlib.sha256(payload).hexdigest()}")


def compare(source: Path, received: Path) -> bool:
    if not source.is_file() or not received.is_file():
        print(f"MISSING: {source} or {received}")
        return False
    expected = source.read_bytes()
    actual = received.read_bytes()
    digest = hashlib.sha256(actual).hexdigest()
    if actual == expected:
        print(f"PASS: {source.name} -> {received.name}, {len(actual)} B, {digest}")
        return True
    position = next(
        (i for i, (a, b) in enumerate(zip(expected, actual)) if a != b),
        min(len(expected), len(actual)),
    )
    print(
        f"FAIL: {source.name} -> {received.name}; "
        f"sent {len(expected)} B, captured {len(actual)} B, "
        f"first mismatch/length boundary at offset {position}"
    )
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("create", "verify"))
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--size", type=int, default=DEFAULT_SIZE)
    args = parser.parse_args()
    if args.command == "create":
        create(args.directory, args.size)
        return 0
    ok_a = compare(args.directory / VECTOR_A, args.directory / CAPTURE_B)
    ok_b = compare(args.directory / VECTOR_B, args.directory / CAPTURE_A)
    return 0 if ok_a and ok_b else 1


if __name__ == "__main__":
    raise SystemExit(main())
