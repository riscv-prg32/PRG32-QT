#!/usr/bin/env python3
"""Check contracts that are intentionally duplicated between code and documentation."""

from __future__ import annotations

import pathlib
import re
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]
SUPPORTED_PLATFORMS = (
    "Windows",
    "Linux",
    "Raspberry Pi",
    "macOS",
    "iOS",
    "Android",
    "Apple TV",
    "Android TV",
)


def fail(message: str) -> None:
    """Record a consistency failure without hiding subsequent diagnostics."""
    print(f"documentation check: {message}", file=sys.stderr)
    failures.append(message)


failures: list[str] = []
runtime = "\n".join(
    (ROOT / path).read_text(encoding="utf-8") for path in ("src/core/Runtime.h", "src/core/Runtime.cpp")
)
architecture = (ROOT / "docs/ARCHITECTURE.md").read_text(encoding="utf-8")
compatibility = (ROOT / "docs/COMPATIBILITY.md").read_text(encoding="utf-8")
readme = (ROOT / "README.md").read_text(encoding="utf-8")
platforms = (ROOT / "docs/PLATFORMS.md").read_text(encoding="utf-8")

# These values are the serialized ABI-table contract written by Runtime::writeAbiTable.
for literal, description in (
    ("139", "host-call count"),
    ("0x260f6136", "current ABI hash"),
    ("0x006427c2", "compatibility hash 0"),
    ("0x6be6e8d0", "compatibility hash 1"),
):
    if literal not in runtime:
        fail(f"Runtime.cpp no longer contains the documented {description} {literal}")
    if literal not in architecture and literal not in compatibility:
        fail(f"the {description} {literal} is absent from architecture/compatibility documentation")

for platform in SUPPORTED_PLATFORMS:
    if platform.casefold() not in readme.casefold():
        fail(f"README omits supported platform {platform}")
    if platform.casefold() not in platforms.casefold():
        fail(f"docs/PLATFORMS.md omits supported platform {platform}")

expected_helpers = {
    "Windows": "scripts/build-windows.ps1",
    "Linux": "scripts/build-linux.sh",
    "Raspberry Pi": "scripts/build-raspbian.sh",
    "macOS": "scripts/build-macos.sh",
    "iOS": "scripts/build-ios.sh",
    "Android": "scripts/build-android.sh",
    "Apple TV": "scripts/build-tvos.sh",
    "Android TV": "scripts/build-android-tv.sh",
}
for platform, helper in expected_helpers.items():
    if not (ROOT / helper).is_file():
        fail(f"{platform} build helper is missing: {helper}")

for document in sorted((ROOT / "docs").glob("*.md")):
    text = document.read_text(encoding="utf-8")
    headings = [line for line in text.splitlines() if re.match(r"^#{1,6} ", line)]
    if not headings or not headings[0].startswith("# "):
        fail(f"{document.relative_to(ROOT)} has no level-one title")
    if re.search(r"^#######", text, re.MULTILINE):
        fail(f"{document.relative_to(ROOT)} contains a heading deeper than level six")

if failures:
    raise SystemExit(1)
print("documentation check: code, platform, and document contracts agree")
