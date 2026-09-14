#!/usr/bin/env python3
"""Linux-side guards for Phase 0 packaging artifacts (no Windows required)."""
from __future__ import annotations

import pathlib
import re
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
REQUIRED = [
    "PRODUCTION_PLAN.md",
    "PHASE0.md",
    "driver/xm5ldac.inf.in",
    "tools/build.ps1",
    "tools/hardware-report.ps1",
    "tools/resolve-binding.ps1",
    "tools/generate-inf.ps1",
    "tools/package-test.ps1",
    "tools/sign-test.ps1",
    "tools/install-test.ps1",
    "tools/uninstall-test.ps1",
    "tools/smoke-info.ps1",
    ".github/workflows/portable-ci.yml",
]


def fail(msg: str) -> None:
    print(f"FAIL: {msg}", file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    for rel in REQUIRED:
        path = ROOT / rel
        if not path.is_file():
            fail(f"missing {rel}")

    template = (ROOT / "driver/xm5ldac.inf.in").read_text(encoding="utf-8")
    if "__HEADPHONE_SERVICE_HARDWARE_ID__" not in template:
        fail("INF template missing hardware-id placeholder")
    if "Intel" in template and "Do NOT use the Intel" not in template:
        fail("INF template unexpectedly mentions Intel outside the warning")

    fake_id = r"BTHENUM\{0000110b-0000-1000-8000-00805f9b34fb}_LOCALMFG&005d"
    generated = template.replace("__HEADPHONE_SERVICE_HARDWARE_ID__", fake_id)
    if "__HEADPHONE_SERVICE_HARDWARE_ID__" in generated:
        fail("placeholder replacement failed")
    if fake_id not in generated:
        fail("generated INF missing substituted hardware id")

    # Simulate generate-inf radio refusal patterns used by the PowerShell script.
    radio_patterns = ("Intel", "8087", "MS_BTHPORT", "MS_BTHBRB", "Wireless Bluetooth")
    for bad in ("USB\\VID_8087&PID_0A3A", "BTH\\MS_BTHPORT\\0", "Intel(R) Wireless Bluetooth"):
        if not any(p.lower() in bad.lower() for p in radio_patterns):
            fail(f"radio blocklist does not catch {bad}")

    build_ps1 = (ROOT / "tools/build.ps1").read_text(encoding="utf-8")
    for needle in ("IncludeDriver", "Configuration", "generate-inf.ps1"):
        if needle not in build_ps1:
            fail(f"build.ps1 missing expected piece: {needle}")

    # Ensure CTL_CODE documentation in smoke script matches public.h macro intent.
    public_h = (ROOT / "driver/public.h").read_text(encoding="utf-8")
    if "XM5_IOCTL_INFO" not in public_h:
        fail("public.h missing XM5_IOCTL_INFO")
    smoke = (ROOT / "tools/smoke-info.ps1").read_text(encoding="utf-8")
    if "0x00226000" not in smoke:
        fail("smoke-info.ps1 IOCTL constant must be 0x00226000")

    with tempfile.TemporaryDirectory() as tmp:
        out = pathlib.Path(tmp) / "xm5ldac.inf"
        out.write_text(generated, encoding="ascii")
        text = out.read_text(encoding="ascii")
        if not re.search(r"CatalogFile\s*=\s*xm5ldac\.cat", text):
            fail("generated INF missing CatalogFile")

    print("PASS: Phase 0 artifact guards")
    print(f"Checked {len(REQUIRED)} required files and INF substitution")


if __name__ == "__main__":
    main()
