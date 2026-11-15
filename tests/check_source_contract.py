#!/usr/bin/env python3
"""Keep the firmware template-free and the Studio project complete."""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1]
firmware = root / "GPT_NRF2401"
for path in list(firmware.glob("*.h")) + list(firmware.glob("*.cpp")):
    code = re.sub(r"/\*.*?\*/|//[^\n]*", "", path.read_text(), flags=re.S)
    if re.search(r"\btemplate\s*<", code):
        raise SystemExit(f"Template declaration forbidden: {path}")
project = ET.parse(firmware / "GPT_NRF2401.cppproj")
compiled = {node.attrib["Include"] for node in project.iter()
            if node.tag.split("}")[-1] == "Compile"}
expected = {path.name for path in firmware.glob("*.cpp")}
if compiled != expected:
    raise SystemExit(f"Studio compilation mismatch: missing={expected - compiled}, stale={compiled - expected}")
print("PASS template-free firmware and complete Studio project")
