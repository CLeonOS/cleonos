#!/usr/bin/env python3
import shutil
import subprocess
import sys
from pathlib import Path


HRB_DIRS = [
    "a", "hello3", "hello4", "hello5", "winhelo", "winhelo2", "winhelo3", "star1", "stars", "stars2",
    "lines", "walk", "noodle", "beepdown", "color", "color2", "sosu", "sosu2", "sosu3", "type",
    "iroha", "chklang", "notrec", "bball", "invader", "calc", "tview", "mmlplay", "gview",
]

RESOURCE_FILES = [
    "euc.txt",
    "mmldata/kirakira.mml",
    "mmldata/fujisan.mml",
    "mmldata/daigo.mml",
    "mmldata/daiku.mml",
    "pictdata/fujisan.jpg",
    "pictdata/night.bmp",
    "nihongo/nihongo.fnt",
]


def copy_file(src: Path, dst: Path) -> bool:
    if not src.is_file():
        return False
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)
    return True


def write_readme(out_dir: Path, text: str) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "README.txt").write_text(text, encoding="utf-8")


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print("usage: build_haribote_assets.py <repo-root> <out-dir>", file=sys.stderr)
        return 2

    root = Path(argv[1]).resolve()
    out_dir = Path(argv[2]).resolve()
    base = root / "build" / "tmp-haribote"
    harib = base / "harib27f"
    make_tool = base / "z_tools" / "make"

    out_dir.mkdir(parents=True, exist_ok=True)

    if not harib.is_dir() or not make_tool.is_file():
        write_readme(
            out_dir,
            "HariboteOS assets are not available in this build.\n"
            "Expected build/tmp-haribote/harib27f and build/tmp-haribote/z_tools/make.\n"
            "Clone/build HariboteOS assets, then rebuild the CLeonOS ramdisk.\n",
        )
        return 0

    try:
        subprocess.run([str(make_tool), "-r", "full"], cwd=str(harib), check=True)
    except subprocess.CalledProcessError as exc:
        write_readme(out_dir, f"HariboteOS asset build failed with exit code {exc.returncode}.\n")
        return exc.returncode

    copied = 0
    if copy_file(harib / "haribote" / "haribote.sys", out_dir / "HARIBOTE.SYS"):
        copied += 1
    if copy_file(harib / "haribote.img", out_dir / "HARIBOTE.IMG"):
        copied += 1

    for dirname in HRB_DIRS:
        src = harib / dirname / f"{dirname}.hrb"
        if copy_file(src, out_dir / f"{dirname.upper()}.HRB"):
            copied += 1

    for rel in RESOURCE_FILES:
        src = harib / rel
        if copy_file(src, out_dir / Path(rel).name.upper()):
            copied += 1

    write_readme(
        out_dir,
        "HariboteOS 0.27f assets for CLeonOS hbos.\n"
        "HARIBOTE.SYS is loaded by /shell/hbos.elf as the hosted Haribote kernel image.\n"
        f"Copied files: {copied}\n",
    )

    if not (out_dir / "HARIBOTE.SYS").is_file():
        print("build_haribote_assets: HARIBOTE.SYS was not produced", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
