#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import re
import sys


def extract_version(root: Path) -> str:
    header = root / "cleonos" / "c" / "include" / "cleonos_version.h"
    text = header.read_text(encoding="utf-8")
    match = re.search(r'^\s*#\s*define\s+CLEONOS_VERSION_STRING\s+"([^"]+)"', text, re.MULTILINE)
    if not match:
        raise RuntimeError(f"CLEONOS_VERSION_STRING not found in {header}")
    return match.group(1)


def write_os_version(out_dir: Path, version: str) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    data = "\n".join(
        (
            "NAME=CLeonOS",
            f"PRETTY_NAME=CLeonOS {version}",
            "ID=cleonos",
            f"VERSION_ID={version}",
            "VERSION_CODENAME=rolling",
            "HOME_URL=https://github.com/CLeonOS/cleonos",
            "BUG_REPORT_URL=https://github.com/CLeonOS/cleonos/issues",
            "KERNEL=CLKS",
        )
    )
    data += "\n"
    for name in ("os-version", "os-release"):
        (out_dir / name).write_text(data, encoding="utf-8")


def main(argv: list[str]) -> int:
    root = Path(argv[1]).resolve() if len(argv) > 1 else Path(__file__).resolve().parent.parent
    out_dir = Path(argv[2]).resolve() if len(argv) > 2 else root / "build" / "x86_64" / "ramdisk_root" / "etc"
    version = extract_version(root)
    write_os_version(out_dir, version)
    print(f"[os-version] generated {out_dir / 'os-version'} ({version})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
