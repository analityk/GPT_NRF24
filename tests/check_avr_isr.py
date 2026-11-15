#!/usr/bin/env python3
"""Check linked AVR interrupt handlers, not guesses from C++ source."""
import argparse
import re
from pathlib import Path

DEFAULT_MAX_SAVED_REGISTERS = 10
CALL_INSTRUCTIONS = {"call", "rcall", "icall", "eicall"}

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("listing", type=Path)
parser.add_argument("--max-pushes", type=int, default=DEFAULT_MAX_SAVED_REGISTERS)
args = parser.parse_args()
functions = re.split(r"(?m)^[0-9a-f]+ <([^>.][^>]*)>:\n", args.listing.read_text())
found = 0
for name, body in zip(functions[1::2], functions[2::2]):
    if not re.fullmatch(r"__vector_\d+", name):
        continue
    found += 1
    opcodes = re.findall(r"(?m)^\s*[0-9a-f]+:\s+(?:[0-9a-f]{2}\s+)+\s*([a-z]+)\b", body)
    calls = CALL_INSTRUCTIONS.intersection(opcodes)
    pushes = opcodes.count("push")
    pops = opcodes.count("pop")
    if calls or pushes != pops or pushes > args.max_pushes or "reti" not in opcodes:
        raise SystemExit(f"FAIL {name}: calls={calls}, push={pushes}, pop={pops}")
    print(f"PASS {name}: no calls, {pushes} stack bytes saved, balanced stack")
if found == 0:
    raise SystemExit("No interrupt handlers found in linked AVR listing")
