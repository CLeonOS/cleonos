from __future__ import annotations

import argparse
import shlex
import sys
from pathlib import Path
from typing import List

from .constants import DEFAULT_MAX_EXEC_DEPTH
from .runner import CLeonOSWineNative, resolve_elf_target, resolve_rootfs
from .state import SharedKernelState


def _encode_cstr(text: str, size: int) -> bytes:
    if size <= 0:
        return b""

    data = text.encode("utf-8", errors="replace")
    if len(data) >= size:
        data = data[: size - 1]
    return data + (b"\x00" * (size - len(data)))


def _normalize_guest_cwd(cwd: str) -> str:
    value = (cwd or "").strip().replace("\\", "/")
    if not value:
        return "/"
    if not value.startswith("/"):
        value = "/" + value
    return value


def _write_command_context(rootfs: Path, guest_path: str, guest_args: List[str], cwd: str) -> None:
    name = Path(guest_path).name
    cmd = name[:-4] if name.lower().endswith(".elf") else name
    arg = " ".join(guest_args)
    ctx_payload = b"".join(
        (
            _encode_cstr(cmd, 32),
            _encode_cstr(arg, 160),
            _encode_cstr(_normalize_guest_cwd(cwd), 192),
        )
    )
    ctx_path = rootfs / "temp" / ".ush_cmd_ctx.bin"
    ctx_path.parent.mkdir(parents=True, exist_ok=True)
    ctx_path.write_bytes(ctx_payload)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="CLeonOS-Wine: run CLeonOS ELF with Unicorn.")
    parser.add_argument("elf", help="Target ELF path. Supports /guest/path or host file path.")
    parser.add_argument("--rootfs", help="Rootfs directory (default: build/x86_64/ramdisk_root).")
    parser.add_argument("--no-kbd", action="store_true", help="Disable host keyboard input pump.")
    parser.add_argument("--fb-window", action="store_true", help="Enable host framebuffer window (pygame backend).")
    parser.add_argument("--fb-scale", type=int, default=2, help="Framebuffer window scale factor (default: 2).")
    parser.add_argument("--fb-max-fps", type=int, default=60, help="Framebuffer present FPS limit (default: 60).")
    parser.add_argument("--fb-hold-ms", type=int, default=2500, help="Keep fb window visible after app exits (ms).")
    parser.add_argument("--argv-line", default="", help="Guest argv as one line (whitespace-separated).")
    parser.add_argument("--cwd", default="/", help="Guest cwd for command-context apps.")
    parser.add_argument("guest_args", nargs="*", help="Guest args (for dash-prefixed args use '--').")
    parser.add_argument("--max-exec-depth", type=int, default=DEFAULT_MAX_EXEC_DEPTH, help="Nested exec depth guard.")
    parser.add_argument("--verbose", action="store_true", help="Enable verbose runner output.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    guest_args = list(args.guest_args or [])
    argv_items: List[str]

    try:
        rootfs = resolve_rootfs(args.rootfs)
        elf_path, guest_path = resolve_elf_target(args.elf, rootfs)
    except Exception as exc:
        print(f"[WINE][ERROR] {exc}", file=sys.stderr)
        return 2

    if len(guest_args) > 0 and guest_args[0] == "--":
        guest_args = guest_args[1:]

    if (args.argv_line or "").strip():
        try:
            guest_args = shlex.split(args.argv_line)
        except Exception:
            guest_args = (args.argv_line or "").split()

    argv_items = [guest_path]
    argv_items.extend(guest_args)

    if guest_args:
        try:
            _write_command_context(rootfs, guest_path, guest_args, args.cwd)
        except Exception as exc:
            print(f"[WINE][WARN] failed to write command context: {exc}", file=sys.stderr)

    if args.verbose:
        print("[WINE] backend=unicorn", file=sys.stderr)
        print(f"[WINE] rootfs={rootfs}", file=sys.stderr)
        print(f"[WINE] elf={elf_path}", file=sys.stderr)
        print(f"[WINE] guest={guest_path}", file=sys.stderr)

    state = SharedKernelState()
    runner = CLeonOSWineNative(
        elf_path=elf_path,
        rootfs=rootfs,
        guest_path_hint=guest_path,
        state=state,
        max_exec_depth=max(1, args.max_exec_depth),
        no_kbd=args.no_kbd,
        verbose=args.verbose,
        top_level=True,
        fb_window=args.fb_window,
        fb_scale=max(1, args.fb_scale),
        fb_max_fps=max(1, args.fb_max_fps),
        fb_hold_ms=max(0, args.fb_hold_ms),
        argv_items=argv_items,
    )
    ret = runner.run()
    if ret is None:
        return 1

    if args.verbose:
        print(f"\n[WINE] exit=0x{ret:016X}", file=sys.stderr)
    return int(ret & 0xFF)
