#!/usr/bin/env python3
"""
CLeonOS menuconfig

Interactive feature selector that writes:
  - configs/menuconfig/.config.json
  - configs/menuconfig/config.cmake
  - configs/menuconfig/.config.clks.json
  - configs/menuconfig/config.clks.cmake
  - configs/menuconfig/.config.cleonos.json
  - configs/menuconfig/config.cleonos.cmake

Design:
  - CLKS options come from configs/menuconfig/clks_features.json
  - User-space app options are discovered dynamically from *_main.c / *_kmain.c
"""

from __future__ import annotations

import argparse
import os
import json
import re
import sys
import textwrap
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Dict, Iterable, List, Optional, Set, Tuple

try:
    import curses
except Exception:
    curses = None

try:
    from PySide6 import QtCore, QtWidgets
except Exception:
    try:
        from PySide2 import QtCore, QtWidgets
    except Exception:
        QtCore = None
        QtWidgets = None


ROOT_DIR = Path(__file__).resolve().parent.parent
APPS_DIR = ROOT_DIR / "cleonos" / "c" / "apps"
MENUCONFIG_DIR = ROOT_DIR / "configs" / "menuconfig"
CLKS_FEATURES_PATH = MENUCONFIG_DIR / "clks_features.json"
CONFIG_JSON_PATH = MENUCONFIG_DIR / ".config.json"
CONFIG_CMAKE_PATH = MENUCONFIG_DIR / "config.cmake"
CONFIG_CLKS_JSON_PATH = MENUCONFIG_DIR / ".config.clks.json"
CONFIG_CLEONOS_JSON_PATH = MENUCONFIG_DIR / ".config.cleonos.json"
CONFIG_CLKS_CMAKE_PATH = MENUCONFIG_DIR / "config.clks.cmake"
CONFIG_CLEONOS_CMAKE_PATH = MENUCONFIG_DIR / "config.cleonos.cmake"


@dataclass(frozen=True)
class OptionItem:
    key: str
    title: str
    description: str
    kind: str
    default: int
    depends_on: str
    selects: Tuple[str, ...]
    implies: Tuple[str, ...]
    group: str


@dataclass
class EvalResult:
    effective: Dict[str, int]
    visible: Dict[str, bool]
    min_required: Dict[str, int]
    max_selectable: Dict[str, int]
    selected_by: Dict[str, List[str]]
    implied_by: Dict[str, List[str]]
    depends_symbols: Dict[str, List[str]]


TRI_N = 0
TRI_M = 1
TRI_Y = 2


def tri_char(value: int) -> str:
    if value >= TRI_Y:
        return "y"
    if value >= TRI_M:
        return "m"
    return "n"


def tri_text(value: int) -> str:
    ch = tri_char(value)
    if ch == "y":
        return "Y"
    if ch == "m":
        return "M"
    return "N"


def normalize_kind(raw: object) -> str:
    text = str(raw or "bool").strip().lower()
    if text in {"tristate", "tri"}:
        return "tristate"
    return "bool"


def _tri_from_bool(value: bool) -> int:
    return TRI_Y if value else TRI_N


def normalize_tri(raw: object, default: int, kind: str) -> int:
    if isinstance(raw, bool):
        value = TRI_Y if raw else TRI_N
    elif isinstance(raw, (int, float)):
        iv = int(raw)
        if iv <= 0:
            value = TRI_N
        elif iv == 1:
            value = TRI_M
        else:
            value = TRI_Y
    elif isinstance(raw, str):
        text = raw.strip().lower()
        if text in {"1", "on", "true", "yes", "y"}:
            value = TRI_Y
        elif text in {"m", "mod", "module"}:
            value = TRI_M
        elif text in {"0", "off", "false", "no", "n"}:
            value = TRI_N
        else:
            value = default
    else:
        value = default

    if kind == "bool":
        return TRI_Y if value == TRI_Y else TRI_N
    if value < TRI_N:
        return TRI_N
    if value > TRI_Y:
        return TRI_Y
    return value


def _tokenize_dep_expr(expr: str) -> List[str]:
    tokens: List[str] = []
    i = 0
    n = len(expr)

    while i < n:
        ch = expr[i]
        if ch.isspace():
            i += 1
            continue
        if ch in "()!":
            tokens.append(ch)
            i += 1
            continue
        if i + 1 < n:
            pair = expr[i : i + 2]
            if pair in {"&&", "||"}:
                tokens.append(pair)
                i += 2
                continue
        m = re.match(r"[A-Za-z_][A-Za-z0-9_]*", expr[i:])
        if m:
            tok = m.group(0)
            tokens.append(tok)
            i += len(tok)
            continue
        raise RuntimeError(f"invalid token in depends expression: {expr!r}")
    return tokens


class _DepExprParser:
    def __init__(self, tokens: List[str], resolver: Callable[[str], int]):
        self.tokens = tokens
        self.pos = 0
        self.resolver = resolver

    def _peek(self) -> Optional[str]:
        if self.pos >= len(self.tokens):
            return None
        return self.tokens[self.pos]

    def _take(self) -> str:
        if self.pos >= len(self.tokens):
            raise RuntimeError("unexpected end of expression")
        tok = self.tokens[self.pos]
        self.pos += 1
        return tok

    def parse(self) -> int:
        value = self._parse_or()
        if self._peek() is not None:
            raise RuntimeError("unexpected token in expression")
        return value

    def _parse_or(self) -> int:
        value = self._parse_and()
        while self._peek() == "||":
            self._take()
            value = max(value, self._parse_and())
        return value

    def _parse_and(self) -> int:
        value = self._parse_unary()
        while self._peek() == "&&":
            self._take()
            value = min(value, self._parse_unary())
        return value

    def _parse_unary(self) -> int:
        tok = self._peek()
        if tok == "!":
            self._take()
            value = self._parse_unary()
            if value == TRI_Y:
                return TRI_N
            if value == TRI_N:
                return TRI_Y
            return TRI_M
        return self._parse_primary()

    def _parse_primary(self) -> int:
        tok = self._take()
        if tok == "(":
            value = self._parse_or()
            if self._take() != ")":
                raise RuntimeError("missing ')' in expression")
            return value

        lowered = tok.lower()
        if lowered == "y":
            return TRI_Y
        if lowered == "m":
            return TRI_M
        if lowered == "n":
            return TRI_N
        return self.resolver(tok)


def eval_dep_expr(expr: str, resolver: Callable[[str], int]) -> int:
    text = (expr or "").strip()
    if not text:
        return TRI_Y
    tokens = _tokenize_dep_expr(text)
    parser = _DepExprParser(tokens, resolver)
    return parser.parse()


def extract_dep_symbols(expr: str) -> List[str]:
    text = (expr or "").strip()
    if not text:
        return []

    out: List[str] = []
    seen: Set[str] = set()
    for tok in _tokenize_dep_expr(text):
        if tok in {"&&", "||", "!", "(", ")"}:
            continue
        low = tok.lower()
        if low in {"y", "m", "n"}:
            continue
        if tok in seen:
            continue
        seen.add(tok)
        out.append(tok)
    return out


def normalize_bool(raw: object, default: bool) -> bool:
    tri_default = TRI_Y if default else TRI_N
    return normalize_tri(raw, tri_default, "bool") == TRI_Y


def _normalize_key_list(raw: object) -> Tuple[str, ...]:
    items: List[str] = []

    if isinstance(raw, str):
        source = re.split(r"[,\s]+", raw.strip())
    elif isinstance(raw, (list, tuple)):
        source = [str(x) for x in raw]
    else:
        return ()

    for item in source:
        token = str(item).strip()
        if not token:
            continue
        items.append(token)

    return tuple(items)


def sanitize_token(name: str) -> str:
    token = re.sub(r"[^A-Za-z0-9]+", "_", name.strip().upper())
    token = token.strip("_")
    return token or "UNKNOWN"


def load_clks_options() -> List[OptionItem]:
    if not CLKS_FEATURES_PATH.exists():
        raise RuntimeError(f"missing CLKS feature file: {CLKS_FEATURES_PATH}")

    raw = json.loads(CLKS_FEATURES_PATH.read_text(encoding="utf-8"))
    if not isinstance(raw, dict) or "features" not in raw or not isinstance(raw["features"], list):
        raise RuntimeError(f"invalid feature format in {CLKS_FEATURES_PATH}")

    options: List[OptionItem] = []
    for entry in raw["features"]:
        if not isinstance(entry, dict):
            continue
        key = str(entry.get("key", "")).strip()
        title = str(entry.get("title", key)).strip()
        description = str(entry.get("description", "")).strip()
        kind = normalize_kind(entry.get("type", "bool"))
        default = normalize_tri(entry.get("default", TRI_Y), TRI_Y, kind)
        depends_on = str(entry.get("depends_on", entry.get("depends", ""))).strip()
        selects = _normalize_key_list(entry.get("select", entry.get("selects", ())))
        implies = _normalize_key_list(entry.get("imply", entry.get("implies", ())))
        group = str(entry.get("group", entry.get("menu", "General"))).strip() or "General"
        if not key:
            continue
        options.append(
            OptionItem(
                key=key,
                title=title,
                description=description,
                kind=kind,
                default=default,
                depends_on=depends_on,
                selects=selects,
                implies=implies,
                group=group,
            )
        )

    if not options:
        raise RuntimeError(f"no CLKS feature options in {CLKS_FEATURES_PATH}")
    return options


def discover_user_apps() -> List[OptionItem]:
    main_paths = sorted(APPS_DIR.glob("*_main.c"))
    kmain_paths = sorted(APPS_DIR.glob("*_kmain.c"))

    kmain_names = set()
    for path in kmain_paths:
        name = path.stem
        if name.endswith("_kmain"):
            kmain_names.add(name[:-6])

    final_apps: List[Tuple[str, str]] = []
    for path in main_paths:
        name = path.stem
        if not name.endswith("_main"):
            continue
        app = name[:-5]
        if app in kmain_names:
            continue
        if app.endswith("drv"):
            section = "driver"
        elif app == "hello":
            section = "root"
        else:
            section = "shell"
        final_apps.append((app, section))

    for app in sorted(kmain_names):
        final_apps.append((app, "system"))

    final_apps.sort(key=lambda item: (item[1], item[0]))

    options: List[OptionItem] = []
    for app, section in final_apps:
        key = f"CLEONOS_USER_APP_{sanitize_token(app)}"
        title = f"{app}.elf [{section}]"
        description = f"Build and package user app '{app}' into ramdisk/{section}."
        options.append(
            OptionItem(
                key=key,
                title=title,
                description=description,
                kind="bool",
                default=TRI_Y,
                depends_on="",
                selects=(),
                implies=(),
                group=section,
            )
        )

    return options


def _load_values_from_json(path: Path) -> Dict[str, int]:
    if not path.exists():
        return {}
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return {}

    if not isinstance(raw, dict):
        return {}

    out: Dict[str, int] = {}
    for key, value in raw.items():
        if not isinstance(key, str):
            continue
        out[key] = normalize_tri(value, TRI_N, "tristate")
    return out


def load_previous_values(include_user: bool) -> Dict[str, int]:
    out: Dict[str, int] = {}
    # Merge order: legacy combined -> split CLKS -> split CLeonOS.
    out.update(_load_values_from_json(CONFIG_JSON_PATH))
    out.update(_load_values_from_json(CONFIG_CLKS_JSON_PATH))
    if include_user:
        out.update(_load_values_from_json(CONFIG_CLEONOS_JSON_PATH))
    return out


def init_values(options: Iterable[OptionItem], previous: Dict[str, int], use_defaults: bool) -> Dict[str, int]:
    values: Dict[str, int] = {}
    for item in options:
        if not use_defaults and item.key in previous:
            values[item.key] = normalize_tri(previous[item.key], item.default, item.kind)
        else:
            values[item.key] = item.default
    return values


def _build_index(options: Iterable[OptionItem]) -> Dict[str, OptionItem]:
    return {item.key: item for item in options}


def _grouped_options(options: List[OptionItem]) -> List[Tuple[str, List[OptionItem]]]:
    groups: Dict[str, List[OptionItem]] = {}
    ordered_names: List[str] = []

    for item in options:
        name = (item.group or "General").strip() or "General"
        if name not in groups:
            groups[name] = []
            ordered_names.append(name)
        groups[name].append(item)

    out: List[Tuple[str, List[OptionItem]]] = []
    for name in ordered_names:
        out.append((name, groups[name]))
    return out


def _group_enabled_count(group_options: List[OptionItem], ev: EvalResult) -> int:
    return sum(1 for item in group_options if ev.effective.get(item.key, item.default) > TRI_N)


def _set_option_if_exists(values: Dict[str, int], option_index: Dict[str, OptionItem], key: str, level: int) -> None:
    if key in values:
        item = option_index.get(key)
        if item is None:
            return
        values[key] = normalize_tri(level, item.default, item.kind)


def _set_all_options(values: Dict[str, int], options: List[OptionItem], level: int) -> None:
    for item in options:
        values[item.key] = normalize_tri(level, item.default, item.kind)


def apply_preset(preset: str, clks_options: List[OptionItem], user_options: List[OptionItem], values: Dict[str, int]) -> None:
    option_index = _build_index(clks_options + user_options)
    preset_name = preset.strip().lower()

    if preset_name == "full":
        _set_all_options(values, clks_options, TRI_Y)
        _set_all_options(values, user_options, TRI_Y)
        return

    if preset_name == "dev":
        _set_all_options(values, clks_options, TRI_Y)
        _set_all_options(values, user_options, TRI_Y)
        _set_option_if_exists(values, option_index, "CLEONOS_CLKS_ENABLE_USERLAND_AUTO_EXEC", TRI_N)
        _set_option_if_exists(values, option_index, "CLEONOS_CLKS_ENABLE_EXEC_SERIAL_LOG", TRI_Y)
        _set_option_if_exists(values, option_index, "CLEONOS_CLKS_ENABLE_PROCFS", TRI_Y)
        _set_option_if_exists(values, option_index, "CLEONOS_CLKS_ENABLE_IDLE_DEBUG_LOG", TRI_Y)
        return

    if preset_name == "minimal":
        _set_all_options(values, clks_options, TRI_Y)
        _set_all_options(values, user_options, TRI_N)

        clks_disable = [
            "CLEONOS_CLKS_ENABLE_AUDIO",
            "CLEONOS_CLKS_ENABLE_MOUSE",
            "CLEONOS_CLKS_ENABLE_DESKTOP",
            "CLEONOS_CLKS_ENABLE_DRIVER_MANAGER",
            "CLEONOS_CLKS_ENABLE_KELF",
            "CLEONOS_CLKS_ENABLE_EXTERNAL_PSF",
            "CLEONOS_CLKS_ENABLE_ELFRUNNER_PROBE",
            "CLEONOS_CLKS_ENABLE_KLOGD_TASK",
            "CLEONOS_CLKS_ENABLE_KWORKER_TASK",
            "CLEONOS_CLKS_ENABLE_BOOT_VIDEO_LOG",
            "CLEONOS_CLKS_ENABLE_PMM_STATS_LOG",
            "CLEONOS_CLKS_ENABLE_HEAP_STATS_LOG",
            "CLEONOS_CLKS_ENABLE_FS_ROOT_LOG",
            "CLEONOS_CLKS_ENABLE_ELFRUNNER_INIT",
            "CLEONOS_CLKS_ENABLE_SYSCALL_TICK_QUERY",
            "CLEONOS_CLKS_ENABLE_TTY_READY_LOG",
            "CLEONOS_CLKS_ENABLE_IDLE_DEBUG_LOG",
            "CLEONOS_CLKS_ENABLE_USER_SYSTEM_APP_PROBE",
            "CLEONOS_CLKS_ENABLE_SCHED_TASK_COUNT_LOG",
        ]
        for key in clks_disable:
            _set_option_if_exists(values, option_index, key, TRI_N)

        clks_enable = [
            "CLEONOS_CLKS_ENABLE_KEYBOARD",
            "CLEONOS_CLKS_ENABLE_USRD_TASK",
            "CLEONOS_CLKS_ENABLE_USERLAND_AUTO_EXEC",
            "CLEONOS_CLKS_ENABLE_HEAP_SELFTEST",
            "CLEONOS_CLKS_ENABLE_SYSTEM_DIR_CHECK",
            "CLEONOS_CLKS_ENABLE_PROCFS",
            "CLEONOS_CLKS_ENABLE_EXEC_SERIAL_LOG",
            "CLEONOS_CLKS_ENABLE_KBD_TTY_SWITCH_HOTKEY",
            "CLEONOS_CLKS_ENABLE_KBD_CTRL_SHORTCUTS",
            "CLEONOS_CLKS_ENABLE_KBD_FORCE_STOP_HOTKEY",
            "CLEONOS_CLKS_ENABLE_USER_INIT_SCRIPT_PROBE",
            "CLEONOS_CLKS_ENABLE_INTERRUPT_READY_LOG",
            "CLEONOS_CLKS_ENABLE_SHELL_MODE_LOG",
        ]
        for key in clks_enable:
            _set_option_if_exists(values, option_index, key, TRI_Y)

        user_enable_tokens = [
            "SHELL",
            "HELP",
            "LS",
            "CD",
            "PWD",
            "CAT",
            "CLEAR",
            "EXIT",
            "EXEC",
            "DMESG",
            "TTY",
            "PID",
            "PS",
            "KILL",
            "JOBS",
            "FG",
            "BG",
            "RESTART",
            "SHUTDOWN",
            "TTYDRV",
        ]
        for token in user_enable_tokens:
            _set_option_if_exists(values, option_index, f"CLEONOS_USER_APP_{token}", TRI_Y)
        return

    raise RuntimeError(f"unknown preset: {preset}")


def _allowed_values(item: OptionItem, min_required: int, max_selectable: int) -> List[int]:
    upper = normalize_tri(max_selectable, TRI_N, item.kind)
    lower = normalize_tri(min_required, TRI_N, item.kind)
    if lower > upper:
        lower = upper

    if item.kind == "tristate":
        base = [TRI_N, TRI_M, TRI_Y]
    else:
        base = [TRI_N, TRI_Y]
    values = [v for v in base if lower <= v <= upper]
    if values:
        return values
    return [lower]


def _choose_select_level(src_value: int, dst_kind: str, is_imply: bool) -> int:
    if src_value <= TRI_N:
        return TRI_N
    if dst_kind == "bool":
        if is_imply:
            return TRI_Y if src_value == TRI_Y else TRI_N
        return TRI_Y
    return src_value


def evaluate_config(options: List[OptionItem], values: Dict[str, int]) -> EvalResult:
    option_index = _build_index(options)
    depends_symbols = {item.key: extract_dep_symbols(item.depends_on) for item in options}

    effective: Dict[str, int] = {}
    for item in options:
        effective[item.key] = normalize_tri(values.get(item.key, item.default), item.default, item.kind)

    visible = {item.key: True for item in options}
    min_required = {item.key: TRI_N for item in options}
    max_selectable = {item.key: TRI_Y for item in options}
    selected_by = {item.key: [] for item in options}
    implied_by = {item.key: [] for item in options}

    max_rounds = max(1, len(options) * 8)
    for _ in range(max_rounds):
        dep_value: Dict[str, int] = {}
        for item in options:
            def _resolver(symbol: str) -> int:
                return effective.get(symbol, TRI_N)

            try:
                dep_value[item.key] = normalize_tri(eval_dep_expr(item.depends_on, _resolver), TRI_Y, "tristate")
            except Exception:
                dep_value[item.key] = TRI_N

        new_visible: Dict[str, bool] = {}
        new_max: Dict[str, int] = {}
        for item in options:
            dep = dep_value[item.key]
            new_visible[item.key] = dep > TRI_N
            if item.kind == "bool":
                new_max[item.key] = TRI_Y if dep == TRI_Y else TRI_N
            else:
                new_max[item.key] = dep

        new_min = {item.key: TRI_N for item in options}
        new_selected_by = {item.key: [] for item in options}
        new_implied_by = {item.key: [] for item in options}

        for src in options:
            src_value = effective[src.key]
            if src_value <= TRI_N:
                continue

            for dst_key in src.selects:
                dst = option_index.get(dst_key)
                if dst is None:
                    continue
                level = _choose_select_level(src_value, dst.kind, is_imply=False)
                if level > new_min[dst_key]:
                    new_min[dst_key] = level
                new_selected_by[dst_key].append(f"{src.key}={tri_char(src_value)}")

            for dst_key in src.implies:
                dst = option_index.get(dst_key)
                if dst is None:
                    continue
                new_implied_by[dst_key].append(f"{src.key}={tri_char(src_value)}")

        changed = False
        next_effective: Dict[str, int] = {}
        for item in options:
            req = normalize_tri(values.get(item.key, item.default), item.default, item.kind)
            upper = normalize_tri(new_max[item.key], TRI_N, item.kind)
            lower = normalize_tri(new_min[item.key], TRI_N, item.kind)
            if lower > upper:
                lower = upper
            eff = req
            if eff < lower:
                eff = lower
            if eff > upper:
                eff = upper
            eff = normalize_tri(eff, item.default, item.kind)
            next_effective[item.key] = eff
            if eff != effective[item.key]:
                changed = True

        effective = next_effective
        visible = new_visible
        min_required = new_min
        max_selectable = new_max
        selected_by = new_selected_by
        implied_by = new_implied_by
        if not changed:
            break

    for key in selected_by:
        selected_by[key].sort()
        implied_by[key].sort()

    return EvalResult(
        effective=effective,
        visible=visible,
        min_required=min_required,
        max_selectable=max_selectable,
        selected_by=selected_by,
        implied_by=implied_by,
        depends_symbols=depends_symbols,
    )


def _option_on(value: int) -> bool:
    return value > TRI_N


def _set_all(values: Dict[str, int], options: List[OptionItem], level: int) -> None:
    for item in options:
        values[item.key] = normalize_tri(level, item.default, item.kind)


def _set_option_value(values: Dict[str, int], item: OptionItem, level: int) -> None:
    values[item.key] = normalize_tri(level, item.default, item.kind)


def _cycle_option_value(values: Dict[str, int], item: OptionItem, evaluation: EvalResult, step: int = 1) -> None:
    allowed = _allowed_values(item, evaluation.min_required[item.key], evaluation.max_selectable[item.key])
    if not allowed:
        return
    current = normalize_tri(values.get(item.key, item.default), item.default, item.kind)
    if current not in allowed:
        current = evaluation.effective.get(item.key, item.default)
        if current not in allowed:
            current = allowed[0]
    pos = allowed.index(current)
    values[item.key] = allowed[(pos + step) % len(allowed)]


def _detail_lines(item: OptionItem, values: Dict[str, int], ev: EvalResult) -> List[str]:
    req = normalize_tri(values.get(item.key, item.default), item.default, item.kind)
    eff = ev.effective.get(item.key, item.default)
    visible = ev.visible.get(item.key, True)
    floor_val = ev.min_required.get(item.key, TRI_N)
    ceil_val = ev.max_selectable.get(item.key, TRI_Y)
    allowed = ",".join(tri_char(v) for v in _allowed_values(item, floor_val, ceil_val))

    lines = [
        f"kind: {item.kind}",
        f"requested: {tri_char(req)}",
        f"effective: {tri_char(eff)}",
        f"visible: {'yes' if visible else 'no'}",
        f"allowed: {allowed}",
        f"depends: {item.depends_on or '<none>'}",
    ]

    symbols = ev.depends_symbols.get(item.key, [])
    if symbols:
        parts = [f"{sym}={tri_char(ev.effective.get(sym, TRI_N))}" for sym in symbols]
        lines.append("depends values: " + ", ".join(parts))
    else:
        lines.append("depends values: <none>")

    sel = ev.selected_by.get(item.key, [])
    imp = ev.implied_by.get(item.key, [])
    lines.append("selected by: " + (", ".join(sel) if sel else "<none>"))
    lines.append("implied by: " + (", ".join(imp) if imp else "<none>"))
    return lines


def _tri_word(value: int) -> str:
    if value >= TRI_Y:
        return "Enabled"
    if value >= TRI_M:
        return "Module"
    return "Disabled"


def _detail_lines_human(item: OptionItem, values: Dict[str, int], ev: EvalResult) -> List[str]:
    req = normalize_tri(values.get(item.key, item.default), item.default, item.kind)
    eff = ev.effective.get(item.key, item.default)
    visible = ev.visible.get(item.key, True)
    floor_val = ev.min_required.get(item.key, TRI_N)
    ceil_val = ev.max_selectable.get(item.key, TRI_Y)
    allowed_vals = _allowed_values(item, floor_val, ceil_val)

    symbols = ev.depends_symbols.get(item.key, [])
    if symbols:
        dep_values = ", ".join(f"{sym}={tri_char(ev.effective.get(sym, TRI_N))}" for sym in symbols)
    else:
        dep_values = "<none>"

    selected_chain = ", ".join(ev.selected_by.get(item.key, [])) or "<none>"
    implied_chain = ", ".join(ev.implied_by.get(item.key, [])) or "<none>"
    allowed_text = "/".join(f"{tri_char(v)}({_tri_word(v)})" for v in allowed_vals)

    return [
        "State:",
        f"  Running now   : {tri_char(eff)} ({_tri_word(eff)})",
        f"  Your choice   : {tri_char(req)} ({_tri_word(req)})",
        f"  Type          : {item.kind}",
        f"  Visible       : {'yes' if visible else 'no'}",
        f"  Allowed now   : {allowed_text}",
        "Why:",
        f"  depends on    : {item.depends_on or '<none>'}",
        f"  depends value : {dep_values}",
        f"  selected by   : {selected_chain}",
        f"  implied by    : {implied_chain}",
        "Notes:",
        "  [a/b] in list means [effective/requested].",
        f"  {item.description}",
    ]


def print_section(title: str, section_options: List[OptionItem], all_options: List[OptionItem], values: Dict[str, int]) -> None:
    ev = evaluate_config(all_options, values)
    print()
    print(f"== {title} ==")
    for idx, item in enumerate(section_options, start=1):
        req = normalize_tri(values.get(item.key, item.default), item.default, item.kind)
        eff = ev.effective.get(item.key, item.default)
        visible = ev.visible.get(item.key, True)
        floor_val = ev.min_required.get(item.key, TRI_N)
        ceil_val = ev.max_selectable.get(item.key, TRI_Y)
        locked = len(_allowed_values(item, floor_val, ceil_val)) <= 1
        flags: List[str] = []
        if not visible:
            flags.append("hidden")
        if locked:
            flags.append("locked")
        if ev.selected_by.get(item.key):
            flags.append("selected")
        if ev.implied_by.get(item.key):
            flags.append("implied")
        flag_text = f" ({', '.join(flags)})" if flags else ""
        print(f"{idx:3d}. [{tri_char(eff)}|{tri_char(req)}] {item.title}{flag_text}")
    print("Commands: <number> cycle, a all->y, n all->n, m all->m, i <n> info, b back")
    print("Legend: [effective|requested], states: y/m/n")


def section_loop(title: str, section_options: List[OptionItem], all_options: List[OptionItem], values: Dict[str, int]) -> None:
    while True:
        ev = evaluate_config(all_options, values)
        print_section(title, section_options, all_options, values)
        raw = input(f"{title}> ").strip()
        if not raw:
            continue
        lower = raw.lower()

        if lower in {"b", "back", "q", "quit"}:
            return
        if lower in {"a", "all", "on"}:
            for item in section_options:
                _set_option_value(values, item, TRI_Y)
            continue
        if lower in {"n", "none", "off"}:
            for item in section_options:
                _set_option_value(values, item, TRI_N)
            continue
        if lower in {"m", "mod", "module"}:
            for item in section_options:
                _set_option_value(values, item, TRI_M)
            continue
        if lower.startswith("i "):
            token = lower[2:].strip()
            if token.isdigit():
                idx = int(token)
                if 1 <= idx <= len(section_options):
                    item = section_options[idx - 1]
                    print()
                    print(f"[{idx}] {item.title}")
                    print(f"key: {item.key}")
                    for line in _detail_lines(item, values, ev):
                        print(line)
                    print(f"desc: {item.description}")
                    continue
            print("invalid info index")
            continue

        if raw.isdigit():
            idx = int(raw)
            if 1 <= idx <= len(section_options):
                item = section_options[idx - 1]
                _cycle_option_value(values, item, ev)
            else:
                print("invalid index")
            continue

        print("unknown command")


def grouped_section_loop(
    title: str,
    section_options: List[OptionItem],
    all_options: List[OptionItem],
    values: Dict[str, int],
) -> None:
    groups = _grouped_options(section_options)

    if len(groups) <= 1:
        section_loop(title, section_options, all_options, values)
        return

    while True:
        ev = evaluate_config(all_options, values)
        print()
        print(f"== {title} / Groups ==")
        print(f"  0. All ({_group_enabled_count(section_options, ev)}/{len(section_options)} enabled)")
        for idx, (name, opts) in enumerate(groups, start=1):
            print(f"{idx:3d}. {name} ({_group_enabled_count(opts, ev)}/{len(opts)} enabled)")
        print("Commands: <number> open, b back")

        raw = input(f"{title}/groups> ").strip().lower()
        if not raw:
            continue
        if raw in {"b", "back", "q", "quit"}:
            return
        if not raw.isdigit():
            print("invalid selection")
            continue

        idx = int(raw)
        if idx == 0:
            section_loop(title, section_options, all_options, values)
            continue
        if 1 <= idx <= len(groups):
            group_name, group_items = groups[idx - 1]
            section_loop(f"{title}/{group_name}", group_items, all_options, values)
            continue
        print("invalid selection")


def _safe_addnstr(stdscr, y: int, x: int, text: str, attr: int = 0) -> None:
    h, w = stdscr.getmaxyx()
    if y < 0 or y >= h or x >= w:
        return
    max_len = max(0, w - x - 1)
    if max_len <= 0:
        return
    try:
        stdscr.addnstr(y, x, text, max_len, attr)
    except Exception:
        pass


def _safe_addch(stdscr, y: int, x: int, ch, attr: int = 0) -> None:
    h, w = stdscr.getmaxyx()
    if y < 0 or y >= h or x < 0 or x >= w:
        return
    try:
        stdscr.addch(y, x, ch, attr)
    except Exception:
        pass


def _curses_theme() -> Dict[str, int]:
    # Reasonable monochrome fallback first.
    theme = {
        "header": curses.A_BOLD,
        "subtitle": curses.A_DIM,
        "panel_border": curses.A_DIM,
        "panel_title": curses.A_BOLD,
        "selected": curses.A_REVERSE | curses.A_BOLD,
        "enabled": curses.A_BOLD,
        "disabled": curses.A_DIM,
        "value_key": curses.A_DIM,
        "value_label": curses.A_BOLD,
        "help": curses.A_DIM,
        "status_ok": curses.A_BOLD,
        "status_warn": curses.A_BOLD,
        "progress_on": curses.A_REVERSE,
        "progress_off": curses.A_DIM,
        "scroll_track": curses.A_DIM,
        "scroll_thumb": curses.A_BOLD,
    }

    if not curses.has_colors():
        return theme

    try:
        curses.start_color()
    except Exception:
        return theme

    try:
        curses.use_default_colors()
    except Exception:
        pass

    # Pair index map
    # 1: Header
    # 2: Subtitle
    # 3: Panel border/title
    # 4: Selected row
    # 5: Enabled accent
    # 6: Disabled accent
    # 7: Footer/help
    # 8: Success/status
    # 9: Warning/status
    # 10: Scroll thumb
    try:
        curses.init_pair(1, curses.COLOR_BLACK, curses.COLOR_CYAN)
        curses.init_pair(2, curses.COLOR_CYAN, -1)
        curses.init_pair(3, curses.COLOR_BLUE, -1)
        curses.init_pair(4, curses.COLOR_WHITE, curses.COLOR_BLUE)
        curses.init_pair(5, curses.COLOR_GREEN, -1)
        curses.init_pair(6, curses.COLOR_RED, -1)
        curses.init_pair(7, curses.COLOR_BLACK, curses.COLOR_WHITE)
        curses.init_pair(8, curses.COLOR_BLACK, curses.COLOR_GREEN)
        curses.init_pair(9, curses.COLOR_BLACK, curses.COLOR_YELLOW)
        curses.init_pair(10, curses.COLOR_MAGENTA, -1)
    except Exception:
        return theme

    theme.update(
        {
            "header": curses.color_pair(1) | curses.A_BOLD,
            "subtitle": curses.color_pair(2) | curses.A_DIM,
            "panel_border": curses.color_pair(3),
            "panel_title": curses.color_pair(3) | curses.A_BOLD,
            "selected": curses.color_pair(4) | curses.A_BOLD,
            "enabled": curses.color_pair(5) | curses.A_BOLD,
            "disabled": curses.color_pair(6) | curses.A_DIM,
            "value_key": curses.color_pair(2) | curses.A_DIM,
            "value_label": curses.A_BOLD,
            "help": curses.color_pair(7),
            "status_ok": curses.color_pair(8) | curses.A_BOLD,
            "status_warn": curses.color_pair(9) | curses.A_BOLD,
            "progress_on": curses.color_pair(5) | curses.A_REVERSE,
            "progress_off": curses.A_DIM,
            "scroll_track": curses.A_DIM,
            "scroll_thumb": curses.color_pair(10) | curses.A_BOLD,
        }
    )
    return theme


def _draw_box(stdscr, y: int, x: int, h: int, w: int, title: str, border_attr: int, title_attr: int) -> None:
    if h < 2 or w < 4:
        return
    right = x + w - 1
    bottom = y + h - 1

    _safe_addch(stdscr, y, x, curses.ACS_ULCORNER, border_attr)
    _safe_addch(stdscr, y, right, curses.ACS_URCORNER, border_attr)
    _safe_addch(stdscr, bottom, x, curses.ACS_LLCORNER, border_attr)
    _safe_addch(stdscr, bottom, right, curses.ACS_LRCORNER, border_attr)

    for col in range(x + 1, right):
        _safe_addch(stdscr, y, col, curses.ACS_HLINE, border_attr)
        _safe_addch(stdscr, bottom, col, curses.ACS_HLINE, border_attr)
    for row in range(y + 1, bottom):
        _safe_addch(stdscr, row, x, curses.ACS_VLINE, border_attr)
        _safe_addch(stdscr, row, right, curses.ACS_VLINE, border_attr)

    if title:
        _safe_addnstr(stdscr, y, x + 2, f" {title} ", title_attr)


def _draw_progress_bar(
    stdscr,
    y: int,
    x: int,
    width: int,
    enabled_count: int,
    total_count: int,
    on_attr: int,
    off_attr: int,
) -> None:
    if width < 8 or total_count <= 0:
        return
    bar_w = width - 8
    if bar_w < 4:
        return
    fill = int((enabled_count * bar_w) / total_count)
    for i in range(bar_w):
        ch = "#" if i < fill else "-"
        attr = on_attr if i < fill else off_attr
        _safe_addch(stdscr, y, x + i, ch, attr)
    _safe_addnstr(stdscr, y, x + bar_w + 1, f"{enabled_count:>3}/{total_count:<3}", off_attr | curses.A_BOLD)


def _option_enabled(ev: EvalResult, item: OptionItem) -> bool:
    return ev.effective.get(item.key, item.default) > TRI_N


def _option_flags(item: OptionItem, ev: EvalResult) -> str:
    flags: List[str] = []
    if not ev.visible.get(item.key, True):
        flags.append("hidden")
    if len(_allowed_values(item, ev.min_required.get(item.key, TRI_N), ev.max_selectable.get(item.key, TRI_Y))) <= 1:
        flags.append("locked")
    if ev.selected_by.get(item.key):
        flags.append("selected")
    if ev.implied_by.get(item.key):
        flags.append("implied")
    if not flags:
        return "-"
    return ",".join(flags)


def _draw_scrollbar(stdscr, y: int, x: int, height: int, total: int, top: int, visible: int, track_attr: int, thumb_attr: int) -> None:
    if height <= 0:
        return
    for r in range(height):
        _safe_addch(stdscr, y + r, x, "|", track_attr)

    if total <= 0 or visible <= 0 or total <= visible:
        for r in range(height):
            _safe_addch(stdscr, y + r, x, "|", thumb_attr)
        return

    thumb_h = max(1, int((visible * height) / total))
    if thumb_h > height:
        thumb_h = height

    max_top = max(1, total - visible)
    max_pos = max(0, height - thumb_h)
    thumb_y = int((top * max_pos) / max_top)

    for r in range(thumb_h):
        _safe_addch(stdscr, y + thumb_y + r, x, "#", thumb_attr)


def _run_ncurses_section(
    stdscr,
    theme: Dict[str, int],
    title: str,
    section_options: List[OptionItem],
    all_options: List[OptionItem],
    values: Dict[str, int],
) -> None:
    selected = 0
    top = 0

    while True:
        ev = evaluate_config(all_options, values)
        stdscr.erase()
        h, w = stdscr.getmaxyx()

        if h < 14 or w < 70:
            _safe_addnstr(stdscr, 0, 0, "Terminal too small for rich UI (need >= 70x14).", theme["status_warn"])
            _safe_addnstr(stdscr, 2, 0, "Resize terminal then press any key, or ESC to go back.")
            key = stdscr.getch()
            if key in (27,):
                return
            continue

        left_w = max(38, int(w * 0.58))
        right_w = w - left_w
        if right_w < 24:
            left_w = w - 24
            right_w = 24

        list_box_y = 2
        list_box_x = 0
        list_box_h = h - 4
        list_box_w = left_w

        detail_box_y = 2
        detail_box_x = left_w
        detail_box_h = h - 4
        detail_box_w = w - left_w

        _safe_addnstr(stdscr, 0, 0, f" CLeonOS menuconfig / {title} ", theme["header"])
        enabled_count = sum(1 for item in section_options if _option_enabled(ev, item))
        module_count = sum(1 for item in section_options if ev.effective.get(item.key, TRI_N) == TRI_M)
        _safe_addnstr(
            stdscr,
            1,
            0,
            f" on:{enabled_count} mod:{module_count} total:{len(section_options)}  |  Space cycle  a/n/m all  Enter/ESC back ",
            theme["subtitle"],
        )

        _draw_box(stdscr, list_box_y, list_box_x, list_box_h, list_box_w, "Options", theme["panel_border"], theme["panel_title"])
        _draw_box(stdscr, detail_box_y, detail_box_x, detail_box_h, detail_box_w, "Details", theme["panel_border"], theme["panel_title"])

        list_inner_y = list_box_y + 1
        list_inner_x = list_box_x + 1
        list_inner_h = list_box_h - 2
        list_inner_w = list_box_w - 2
        visible = max(1, list_inner_h)

        if selected < 0:
            selected = 0
        if selected >= len(section_options):
            selected = max(0, len(section_options) - 1)

        if selected < top:
            top = selected
        if selected >= top + visible:
            top = selected - visible + 1
        if top < 0:
            top = 0

        for row in range(visible):
            idx = top + row
            if idx >= len(section_options):
                break
            item = section_options[idx]
            req = normalize_tri(values.get(item.key, item.default), item.default, item.kind)
            eff = ev.effective.get(item.key, item.default)
            flags = _option_flags(item, ev)
            prefix = ">" if idx == selected else " "
            line = f"{prefix} {idx + 1:03d} [{tri_char(eff)}|{tri_char(req)}] {item.title}"
            if flags != "-":
                line += f" [{flags}]"
            base_attr = theme["enabled"] if eff > TRI_N else theme["disabled"]
            attr = theme["selected"] if idx == selected else base_attr
            _safe_addnstr(stdscr, list_inner_y + row, list_inner_x, line, attr)

        _draw_scrollbar(
            stdscr,
            list_inner_y,
            list_box_x + list_box_w - 2,
            list_inner_h,
            len(section_options),
            top,
            visible,
            theme["scroll_track"],
            theme["scroll_thumb"],
        )

        if section_options:
            cur = section_options[selected]
            detail_inner_y = detail_box_y + 1
            detail_inner_x = detail_box_x + 2
            detail_inner_w = detail_box_w - 4
            detail_inner_h = detail_box_h - 2

            eff = ev.effective.get(cur.key, cur.default)
            req = normalize_tri(values.get(cur.key, cur.default), cur.default, cur.kind)
            state_text = f"effective={tri_char(eff)} requested={tri_char(req)} kind={cur.kind}"
            state_attr = theme["status_ok"] if eff > TRI_N else theme["status_warn"]

            _safe_addnstr(stdscr, detail_inner_y + 0, detail_inner_x, cur.title, theme["value_label"])
            _safe_addnstr(stdscr, detail_inner_y + 1, detail_inner_x, cur.key, theme["value_key"])
            _safe_addnstr(stdscr, detail_inner_y + 2, detail_inner_x, f"State: {state_text}", state_attr)
            _safe_addnstr(
                stdscr,
                detail_inner_y + 3,
                detail_inner_x,
                f"Item: {selected + 1}/{len(section_options)}  flags: {_option_flags(cur, ev)}",
                theme["value_label"],
            )
            _draw_progress_bar(
                stdscr,
                detail_inner_y + 4,
                detail_inner_x,
                max(12, detail_inner_w),
                enabled_count,
                max(1, len(section_options)),
                theme["progress_on"],
                theme["progress_off"],
            )

            desc_title_y = detail_inner_y + 6
            _safe_addnstr(stdscr, desc_title_y, detail_inner_x, "Details:", theme["value_label"])
            raw_lines = _detail_lines_human(cur, values, ev)
            wrapped_lines: List[str] = []
            wrap_width = max(12, detail_inner_w)
            for raw_line in raw_lines:
                if not raw_line:
                    wrapped_lines.append("")
                    continue
                chunks = textwrap.wrap(raw_line, wrap_width)
                if chunks:
                    wrapped_lines.extend(chunks)
                else:
                    wrapped_lines.append(raw_line)
            max_desc_lines = max(1, detail_inner_h - 8)
            for i, part in enumerate(wrapped_lines[:max_desc_lines]):
                _safe_addnstr(stdscr, desc_title_y + 1 + i, detail_inner_x, part, 0)

        _safe_addnstr(stdscr, h - 1, 0, " Space:cycle  a:all-y  n:all-n  m:all-m  Enter/ESC:back ", theme["help"])

        stdscr.refresh()
        key = stdscr.getch()

        if key in (27, ord("q"), ord("Q"), curses.KEY_LEFT, curses.KEY_ENTER, 10, 13):
            return
        if key in (curses.KEY_UP, ord("k"), ord("K")):
            selected -= 1
            continue
        if key in (curses.KEY_DOWN, ord("j"), ord("J")):
            selected += 1
            continue
        if key == curses.KEY_PPAGE:
            selected -= visible
            continue
        if key == curses.KEY_NPAGE:
            selected += visible
            continue
        if key == curses.KEY_HOME:
            selected = 0
            continue
        if key == curses.KEY_END:
            selected = max(0, len(section_options) - 1)
            continue
        if key == ord(" "):
            if section_options:
                item = section_options[selected]
                _cycle_option_value(values, item, ev)
            continue
        if key in (ord("a"), ord("A")):
            _set_all(values, section_options, TRI_Y)
            continue
        if key in (ord("n"), ord("N")):
            _set_all(values, section_options, TRI_N)
            continue
        if key in (ord("m"), ord("M")):
            _set_all(values, section_options, TRI_M)
            continue


def _run_ncurses_grouped_section(
    stdscr,
    theme: Dict[str, int],
    title: str,
    section_options: List[OptionItem],
    all_options: List[OptionItem],
    values: Dict[str, int],
) -> None:
    groups = _grouped_options(section_options)
    if len(groups) <= 1:
        _run_ncurses_section(stdscr, theme, title, section_options, all_options, values)
        return

    selected = 0

    while True:
        ev = evaluate_config(all_options, values)
        stdscr.erase()
        h, w = stdscr.getmaxyx()
        items: List[Tuple[str, List[OptionItem]]] = [("All", section_options)] + groups

        if h < 12 or w < 56:
            _safe_addnstr(stdscr, 0, 0, "Terminal too small for grouped view (need >= 56x12).", theme["status_warn"])
            _safe_addnstr(stdscr, 2, 0, "Resize terminal then press any key, or ESC to go back.")
            key = stdscr.getch()
            if key in (27,):
                return
            continue

        _safe_addnstr(stdscr, 0, 0, f" CLeonOS menuconfig / {title} / Groups ", theme["header"])
        _safe_addnstr(stdscr, 1, 0, " Enter: open group  ESC: back ", theme["subtitle"])

        _draw_box(stdscr, 2, 0, h - 4, w, "CLKS Groups", theme["panel_border"], theme["panel_title"])

        if selected < 0:
            selected = 0
        if selected >= len(items):
            selected = len(items) - 1

        for i, (name, opts) in enumerate(items):
            row = 4 + i
            if row >= h - 2:
                break
            on_count = _group_enabled_count(opts, ev)
            line = f"{'>' if i == selected else ' '} {i:02d}  {name}  ({on_count}/{len(opts)} enabled)"
            attr = theme["selected"] if i == selected else theme["value_label"]
            _safe_addnstr(stdscr, row, 2, line, attr)

        _safe_addnstr(stdscr, h - 1, 0, " Arrows/jk move  Enter open  ESC back ", theme["help"])
        stdscr.refresh()
        key = stdscr.getch()

        if key in (27, ord("q"), ord("Q"), curses.KEY_LEFT):
            return
        if key in (curses.KEY_UP, ord("k"), ord("K")):
            selected = (selected - 1) % len(items)
            continue
        if key in (curses.KEY_DOWN, ord("j"), ord("J")):
            selected = (selected + 1) % len(items)
            continue
        if key in (curses.KEY_ENTER, 10, 13):
            name, opts = items[selected]
            if name == "All":
                _run_ncurses_section(stdscr, theme, title, opts, all_options, values)
            else:
                _run_ncurses_section(stdscr, theme, f"{title}/{name}", opts, all_options, values)
            continue


def _run_ncurses_main(stdscr, clks_options: List[OptionItem], user_options: List[OptionItem], values: Dict[str, int]) -> bool:
    theme = _curses_theme()
    all_options = clks_options + user_options
    try:
        curses.curs_set(0)
    except Exception:
        pass
    stdscr.keypad(True)
    selected = 0

    while True:
        stdscr.erase()
        h, w = stdscr.getmaxyx()

        ev = evaluate_config(all_options, values)
        clks_on = sum(1 for item in clks_options if _option_enabled(ev, item))
        user_on = sum(1 for item in user_options if _option_enabled(ev, item))
        total_items = len(clks_options) + len(user_options)
        total_on = clks_on + user_on

        menu_entries: List[Tuple[str, str]] = [
            ("clks", f"CLKS features ({clks_on}/{len(clks_options)} enabled)"),
        ]
        if user_options:
            menu_entries.append(("user", f"User apps ({user_on}/{len(user_options)} enabled)"))
        else:
            menu_entries.append(("user-disabled", "User apps (CLKS-only mode)"))
        menu_entries.append(("save", "Save and Exit"))
        menu_entries.append(("quit", "Quit without Saving"))

        if h < 12 or w < 58:
            _safe_addnstr(stdscr, 0, 0, "Terminal too small for menuconfig (need >= 58x12).", theme["status_warn"])
            _safe_addnstr(stdscr, 2, 0, "Resize terminal then press any key.")
            stdscr.getch()
            continue

        _safe_addnstr(stdscr, 0, 0, " CLeonOS menuconfig ", theme["header"])
        _safe_addnstr(stdscr, 1, 0, " Stylish ncurses UI  |  Enter: open/select  s: save  q: quit ", theme["subtitle"])

        _draw_box(stdscr, 2, 0, h - 5, w, "Main", theme["panel_border"], theme["panel_title"])

        base = 4
        for i, (_action, text) in enumerate(menu_entries):
            prefix = ">" if i == selected else " "
            row_text = f"{prefix} {text}"
            attr = theme["selected"] if i == selected else theme["value_label"]
            _safe_addnstr(stdscr, base + i, 2, row_text, attr)

        _safe_addnstr(stdscr, base + 6, 2, "Global Progress:", theme["value_label"])
        _draw_progress_bar(
            stdscr,
            base + 7,
            2,
            max(18, w - 6),
            total_on,
            max(1, total_items),
            theme["progress_on"],
            theme["progress_off"],
        )

        _safe_addnstr(stdscr, h - 2, 0, " Arrows/jk move  Enter select  s save  q quit ", theme["help"])
        if user_options:
            _safe_addnstr(stdscr, h - 1, 0, " Tip: open CLKS/USER section then use Space to toggle options. ", theme["help"])
        else:
            _safe_addnstr(stdscr, h - 1, 0, " Tip: CLKS-only mode, open CLKS section then use Space to toggle options. ", theme["help"])
        stdscr.refresh()

        key = stdscr.getch()

        if key in (ord("q"), ord("Q"), 27):
            return False
        if key in (ord("s"), ord("S")):
            return True
        if key in (curses.KEY_UP, ord("k"), ord("K")):
            selected = (selected - 1) % len(menu_entries)
            continue
        if key in (curses.KEY_DOWN, ord("j"), ord("J")):
            selected = (selected + 1) % len(menu_entries)
            continue
        if key in (curses.KEY_ENTER, 10, 13):
            action = menu_entries[selected][0]
            if action == "clks":
                _run_ncurses_grouped_section(stdscr, theme, "CLKS", clks_options, all_options, values)
            elif action == "user":
                _run_ncurses_section(stdscr, theme, "USER", user_options, all_options, values)
            elif action == "save":
                return True
            elif action == "quit":
                return False
            else:
                continue
            continue


def interactive_menu_ncurses(clks_options: List[OptionItem], user_options: List[OptionItem], values: Dict[str, int]) -> bool:
    if curses is None:
        raise RuntimeError("python curses module unavailable (install python3-curses / ncurses)")
    if "TERM" not in os.environ or not os.environ["TERM"]:
        raise RuntimeError("TERM is not set; cannot start ncurses UI")
    return bool(curses.wrapper(lambda stdscr: _run_ncurses_main(stdscr, clks_options, user_options, values)))


def interactive_menu_gui(clks_options: List[OptionItem], user_options: List[OptionItem], values: Dict[str, int]) -> bool:
    if QtWidgets is None or QtCore is None:
        raise RuntimeError("python PySide unavailable (install PySide6, or use --plain)")

    if os.name != "nt" and not os.environ.get("DISPLAY") and not os.environ.get("WAYLAND_DISPLAY"):
        raise RuntimeError("GUI mode requires a desktop display (DISPLAY/WAYLAND_DISPLAY)")

    app = QtWidgets.QApplication.instance()
    owns_app = False

    if app is None:
        app = QtWidgets.QApplication(["menuconfig-gui"])
        owns_app = True

    qt_horizontal = getattr(QtCore.Qt, "Horizontal", QtCore.Qt.Orientation.Horizontal)
    qt_item_enabled = getattr(QtCore.Qt, "ItemIsEnabled", QtCore.Qt.ItemFlag.ItemIsEnabled)
    qt_item_selectable = getattr(QtCore.Qt, "ItemIsSelectable", QtCore.Qt.ItemFlag.ItemIsSelectable)

    resize_to_contents = getattr(
        QtWidgets.QHeaderView,
        "ResizeToContents",
        QtWidgets.QHeaderView.ResizeMode.ResizeToContents,
    )
    stretch_mode = getattr(
        QtWidgets.QHeaderView,
        "Stretch",
        QtWidgets.QHeaderView.ResizeMode.Stretch,
    )
    select_rows = getattr(
        QtWidgets.QAbstractItemView,
        "SelectRows",
        QtWidgets.QAbstractItemView.SelectionBehavior.SelectRows,
    )
    extended_selection = getattr(
        QtWidgets.QAbstractItemView,
        "ExtendedSelection",
        QtWidgets.QAbstractItemView.SelectionMode.ExtendedSelection,
    )

    result = {"save": False}

    dialog = QtWidgets.QDialog()
    dialog.setWindowTitle("CLeonOS menuconfig (PySide)")
    dialog.resize(1180, 760)
    dialog.setMinimumSize(920, 560)

    if os.name == "nt":
        dialog.setWindowState(dialog.windowState() | QtCore.Qt.WindowMaximized)

    root_layout = QtWidgets.QVBoxLayout(dialog)
    root_layout.setContentsMargins(12, 10, 12, 12)
    root_layout.setSpacing(8)

    header_title = QtWidgets.QLabel("CLeonOS menuconfig")
    header_font = header_title.font()
    header_font.setPointSize(header_font.pointSize() + 4)
    header_font.setBold(True)
    header_title.setFont(header_font)
    root_layout.addWidget(header_title)

    if user_options:
        root_layout.addWidget(QtWidgets.QLabel("Window mode (PySide): configure CLKS features and user apps, then save."))
    else:
        root_layout.addWidget(QtWidgets.QLabel("Window mode (PySide): CLKS-only mode (user app options unavailable)."))

    summary_label = QtWidgets.QLabel("")
    root_layout.addWidget(summary_label)

    tabs = QtWidgets.QTabWidget()
    root_layout.addWidget(tabs, 1)
    all_options = clks_options + user_options

    def update_summary() -> None:
        ev = evaluate_config(all_options, values)
        clks_on = sum(1 for item in clks_options if ev.effective.get(item.key, item.default) > TRI_N)
        user_on = sum(1 for item in user_options if ev.effective.get(item.key, item.default) > TRI_N)
        total = len(clks_options) + len(user_options)
        summary_label.setText(
            f"CLKS: {clks_on}/{len(clks_options)} on    "
            f"User: {user_on}/{len(user_options)} on    "
            f"Total: {clks_on + user_on}/{total}"
        )

    class _SectionPanel(QtWidgets.QWidget):
        def __init__(self, title: str, options: List[OptionItem]):
            super().__init__()
            self.options = options
            self._updating = False

            layout = QtWidgets.QVBoxLayout(self)
            layout.setContentsMargins(0, 0, 0, 0)
            layout.setSpacing(8)

            toolbar = QtWidgets.QHBoxLayout()
            title_label = QtWidgets.QLabel(title)
            title_font = title_label.font()
            title_font.setBold(True)
            title_label.setFont(title_font)
            toolbar.addWidget(title_label)
            toolbar.addStretch(1)

            toggle_btn = QtWidgets.QPushButton("Cycle Selected")
            set_y_btn = QtWidgets.QPushButton("Set Y")
            set_m_btn = QtWidgets.QPushButton("Set M")
            set_n_btn = QtWidgets.QPushButton("Set N")
            enable_all_btn = QtWidgets.QPushButton("All Y")
            disable_all_btn = QtWidgets.QPushButton("All N")
            toolbar.addWidget(enable_all_btn)
            toolbar.addWidget(disable_all_btn)
            toolbar.addWidget(set_m_btn)
            toolbar.addWidget(set_y_btn)
            toolbar.addWidget(set_n_btn)
            toolbar.addWidget(toggle_btn)
            layout.addLayout(toolbar)

            splitter = QtWidgets.QSplitter(qt_horizontal)
            layout.addWidget(splitter, 1)

            left = QtWidgets.QWidget()
            left_layout = QtWidgets.QVBoxLayout(left)
            left_layout.setContentsMargins(0, 0, 0, 0)
            self.table = QtWidgets.QTableWidget(len(options), 3)
            self.table.setHorizontalHeaderLabels(["Value", "Option", "Status"])
            self.table.verticalHeader().setVisible(False)
            self.table.horizontalHeader().setSectionResizeMode(0, resize_to_contents)
            self.table.horizontalHeader().setSectionResizeMode(1, stretch_mode)
            self.table.horizontalHeader().setSectionResizeMode(2, resize_to_contents)
            self.table.setSelectionBehavior(select_rows)
            self.table.setSelectionMode(extended_selection)
            self.table.setAlternatingRowColors(True)
            left_layout.addWidget(self.table)
            splitter.addWidget(left)

            right = QtWidgets.QWidget()
            right_layout = QtWidgets.QVBoxLayout(right)
            right_layout.setContentsMargins(0, 0, 0, 0)
            self.state_label = QtWidgets.QLabel("State: -")
            self.key_label = QtWidgets.QLabel("Key: -")
            self.detail_text = QtWidgets.QPlainTextEdit()
            self.detail_text.setReadOnly(True)
            right_layout.addWidget(self.state_label)
            right_layout.addWidget(self.key_label)
            right_layout.addWidget(self.detail_text, 1)
            splitter.addWidget(right)
            splitter.setStretchFactor(0, 3)
            splitter.setStretchFactor(1, 2)

            toggle_btn.clicked.connect(self.toggle_selected)
            set_y_btn.clicked.connect(lambda: self.set_selected(TRI_Y))
            set_m_btn.clicked.connect(lambda: self.set_selected(TRI_M))
            set_n_btn.clicked.connect(lambda: self.set_selected(TRI_N))
            enable_all_btn.clicked.connect(self.enable_all)
            disable_all_btn.clicked.connect(self.disable_all)
            self.table.itemSelectionChanged.connect(self._on_selection_changed)
            self.table.itemDoubleClicked.connect(self._on_item_activated)

            self.refresh(keep_selection=False)
            if self.options:
                self.table.selectRow(0)
                self._show_detail(0)

        def _selected_rows(self) -> List[int]:
            rows = []
            model = self.table.selectionModel()

            if model is None:
                return rows

            for idx in model.selectedRows():
                row = idx.row()
                if row not in rows:
                    rows.append(row)

            rows.sort()
            return rows

        def _show_detail(self, row: int) -> None:
            if row < 0 or row >= len(self.options):
                self.state_label.setText("State: -")
                self.key_label.setText("Key: -")
                self.detail_text.setPlainText("")
                return

            item = self.options[row]
            ev = evaluate_config(all_options, values)
            eff = ev.effective.get(item.key, item.default)
            req = normalize_tri(values.get(item.key, item.default), item.default, item.kind)
            self.state_label.setText(
                f"State: eff={tri_char(eff)} req={tri_char(req)} kind={item.kind} flags={_option_flags(item, ev)}"
            )
            self.key_label.setText(f"Key: {item.key}")
            self.detail_text.setPlainText("\n".join([item.title, ""] + _detail_lines(item, values, ev) + ["", item.description]))

        def _on_selection_changed(self) -> None:
            rows = self._selected_rows()

            if len(rows) == 1:
                self._show_detail(rows[0])
                return

            if len(rows) > 1:
                self.state_label.setText(f"State: {len(rows)} items selected")
                self.key_label.setText("Key: <multiple>")
                self.detail_text.setPlainText("Multiple options selected.\nUse Cycle/Set buttons to update selected entries.")
                return

            self._show_detail(-1)

        def _on_item_activated(self, changed_item) -> None:
            if self._updating or changed_item is None:
                return
            row = changed_item.row()
            if row < 0 or row >= len(self.options):
                return
            ev = evaluate_config(all_options, values)
            _cycle_option_value(values, self.options[row], ev)
            self.refresh(keep_selection=True)

        def refresh(self, keep_selection: bool = True) -> None:
            prev_rows = self._selected_rows() if keep_selection else []
            self._updating = True

            self.table.setRowCount(len(self.options))
            ev = evaluate_config(all_options, values)

            for row, item in enumerate(self.options):
                req = normalize_tri(values.get(item.key, item.default), item.default, item.kind)
                eff = ev.effective.get(item.key, item.default)
                value_item = self.table.item(row, 0)
                if value_item is None:
                    value_item = QtWidgets.QTableWidgetItem("")
                    value_item.setFlags(qt_item_enabled | qt_item_selectable)
                    self.table.setItem(row, 0, value_item)
                value_item.setText(f"{tri_char(eff)} (req:{tri_char(req)})")

                title_item = self.table.item(row, 1)
                if title_item is None:
                    title_item = QtWidgets.QTableWidgetItem(item.title)
                    title_item.setFlags(qt_item_enabled | qt_item_selectable)
                    self.table.setItem(row, 1, title_item)
                else:
                    title_item.setText(item.title)

                status_item = self.table.item(row, 2)
                if status_item is None:
                    status_item = QtWidgets.QTableWidgetItem("")
                    status_item.setFlags(qt_item_enabled | qt_item_selectable)
                    self.table.setItem(row, 2, status_item)
                status_item.setText(_option_flags(item, ev))

            self._updating = False

            self.table.clearSelection()
            if keep_selection:
                for row in prev_rows:
                    if 0 <= row < len(self.options):
                        self.table.selectRow(row)

            self._on_selection_changed()
            update_summary()

        def toggle_selected(self) -> None:
            rows = self._selected_rows()
            if not rows:
                return

            for row in rows:
                item = self.options[row]
                ev = evaluate_config(all_options, values)
                _cycle_option_value(values, item, ev)
            self.refresh(keep_selection=True)

        def set_selected(self, state: int) -> None:
            rows = self._selected_rows()
            if not rows:
                return
            for row in rows:
                item = self.options[row]
                _set_option_value(values, item, state)
            self.refresh(keep_selection=True)

        def enable_all(self) -> None:
            for item in self.options:
                _set_option_value(values, item, TRI_Y)
            self.refresh(keep_selection=False)

        def disable_all(self) -> None:
            for item in self.options:
                _set_option_value(values, item, TRI_N)
            self.refresh(keep_selection=False)

    clks_groups = _grouped_options(clks_options)
    if len(clks_groups) <= 1:
        clks_panel = _SectionPanel("CLKS Features", clks_options)
    else:
        clks_panel = QtWidgets.QWidget()
        clks_layout = QtWidgets.QVBoxLayout(clks_panel)
        clks_layout.setContentsMargins(0, 0, 0, 0)
        clks_layout.setSpacing(6)
        clks_tabs = QtWidgets.QTabWidget()
        clks_layout.addWidget(clks_tabs, 1)
        clks_tabs.addTab(_SectionPanel("CLKS Features / All", clks_options), "All")
        for group_name, group_items in clks_groups:
            clks_tabs.addTab(_SectionPanel(f"CLKS Features / {group_name}", group_items), group_name)

    tabs.addTab(clks_panel, "CLKS")
    if user_options:
        user_panel = _SectionPanel("User Apps", user_options)
        tabs.addTab(user_panel, "USER")
    update_summary()

    footer = QtWidgets.QHBoxLayout()
    footer.addWidget(QtWidgets.QLabel("Tip: double-click a row to cycle, or use Set/Cycle buttons."))
    footer.addStretch(1)

    save_btn = QtWidgets.QPushButton("Save and Exit")
    quit_btn = QtWidgets.QPushButton("Quit without Saving")
    footer.addWidget(save_btn)
    footer.addWidget(quit_btn)
    root_layout.addLayout(footer)

    def _on_save() -> None:
        result["save"] = True
        dialog.accept()

    def _on_quit() -> None:
        result["save"] = False
        dialog.reject()

    save_btn.clicked.connect(_on_save)
    quit_btn.clicked.connect(_on_quit)

    dialog.exec()

    if owns_app:
        app.quit()

    return result["save"]


def _write_json_config(path: Path, values: Dict[str, int], options: List[OptionItem]) -> None:
    output_values: Dict[str, object] = {}
    for item in options:
        if item.key not in values:
            continue
        value = normalize_tri(values[item.key], item.default, item.kind)
        if item.kind == "bool":
            output_values[item.key] = value == TRI_Y
        else:
            output_values[item.key] = tri_char(value)

    path.write_text(
        json.dumps(output_values, ensure_ascii=True, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def _write_cmake_config(path: Path, values: Dict[str, int], options: List[OptionItem], loaded_var: str) -> None:
    lines = [
        "# Auto-generated by scripts/menuconfig.py",
        "# Do not edit manually unless you know what you are doing.",
        f'set({loaded_var} ON CACHE BOOL "CLeonOS menuconfig loaded" FORCE)',
    ]
    for item in options:
        value = normalize_tri(values.get(item.key, item.default), item.default, item.kind)
        if item.kind == "bool":
            cmake_value = "ON" if value == TRI_Y else "OFF"
            lines.append(f'set({item.key} {cmake_value} CACHE BOOL "{item.title}" FORCE)')
        else:
            cmake_value = tri_char(value).upper()
            lines.append(f'set({item.key} "{cmake_value}" CACHE STRING "{item.title}" FORCE)')
            lines.append(
                f'set({item.key}_IS_ENABLED {"ON" if value > TRI_N else "OFF"} '
                f'CACHE BOOL "{item.title} enabled(y|m)" FORCE)'
            )

    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_outputs(all_values: Dict[str, int], clks_options: List[OptionItem], user_options: List[OptionItem]) -> None:
    MENUCONFIG_DIR.mkdir(parents=True, exist_ok=True)
    ordered_options = clks_options + user_options

    _write_json_config(CONFIG_JSON_PATH, all_values, ordered_options)
    _write_json_config(CONFIG_CLKS_JSON_PATH, all_values, clks_options)
    _write_cmake_config(CONFIG_CLKS_CMAKE_PATH, all_values, clks_options, "CLEONOS_MENUCONFIG_CLKS_LOADED")

    if user_options:
        _write_json_config(CONFIG_CLEONOS_JSON_PATH, all_values, user_options)
        _write_cmake_config(
            CONFIG_CLEONOS_CMAKE_PATH,
            all_values,
            user_options,
            "CLEONOS_MENUCONFIG_CLEONOS_LOADED",
        )
    else:
        if CONFIG_CLEONOS_JSON_PATH.exists():
            CONFIG_CLEONOS_JSON_PATH.unlink()
        if CONFIG_CLEONOS_CMAKE_PATH.exists():
            CONFIG_CLEONOS_CMAKE_PATH.unlink()

    # Backward-compatible aggregator for existing CMake include path.
    lines = [
        "# Auto-generated by scripts/menuconfig.py",
        "# Backward-compatible aggregate include.",
        'set(CLEONOS_MENUCONFIG_LOADED ON CACHE BOOL "CLeonOS menuconfig loaded" FORCE)',
        'include("${CMAKE_CURRENT_LIST_DIR}/config.clks.cmake" OPTIONAL)',
        'include("${CMAKE_CURRENT_LIST_DIR}/config.cleonos.cmake" OPTIONAL)',
    ]
    CONFIG_CMAKE_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")


def show_summary(clks_options: List[OptionItem], user_options: List[OptionItem], values: Dict[str, int]) -> None:
    all_options = clks_options + user_options
    ev = evaluate_config(all_options, values)
    clks_on = sum(1 for item in clks_options if ev.effective.get(item.key, item.default) > TRI_N)
    user_on = sum(1 for item in user_options if ev.effective.get(item.key, item.default) > TRI_N)
    clks_m = sum(1 for item in clks_options if ev.effective.get(item.key, item.default) == TRI_M)
    user_m = sum(1 for item in user_options if ev.effective.get(item.key, item.default) == TRI_M)
    print()
    print("========== CLeonOS menuconfig ==========")
    print(f"1) CLKS features : on={clks_on} m={clks_m} total={len(clks_options)}")
    if user_options:
        print(f"2) User features : on={user_on} m={user_m} total={len(user_options)}")
    else:
        print("2) User features : unavailable (CLKS-only mode)")
    print("s) Save and exit")
    print("q) Quit without saving")


def interactive_menu(clks_options: List[OptionItem], user_options: List[OptionItem], values: Dict[str, int]) -> bool:
    all_options = clks_options + user_options
    has_user = len(user_options) > 0
    while True:
        show_summary(clks_options, user_options, values)
        choice = input("Select> ").strip().lower()
        if choice == "1":
            grouped_section_loop("CLKS", clks_options, all_options, values)
            continue
        if choice == "2" and has_user:
            section_loop("USER", user_options, all_options, values)
            continue
        if choice == "2" and not has_user:
            print("user features unavailable in CLKS-only mode")
            continue
        if choice in {"s", "save"}:
            return True
        if choice in {"q", "quit"}:
            return False
        print("unknown selection")


def parse_set_overrides(values: Dict[str, int], option_index: Dict[str, OptionItem], kv_pairs: List[str]) -> None:
    for pair in kv_pairs:
        if "=" not in pair:
            raise RuntimeError(f"invalid --set entry: {pair!r}, expected KEY=Y|M|N")
        key, raw = pair.split("=", 1)
        key = key.strip()
        if not key:
            raise RuntimeError(f"invalid --set entry: {pair!r}, empty key")
        item = option_index.get(key)
        if item is None:
            values[key] = normalize_tri(raw, TRI_N, "tristate")
        else:
            values[key] = normalize_tri(raw, item.default, item.kind)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="CLeonOS menuconfig")
    parser.add_argument("--defaults", action="store_true", help="ignore previous .config and use defaults")
    parser.add_argument("--non-interactive", action="store_true", help="save config without opening interactive menu")
    parser.add_argument("--plain", action="store_true", help="use legacy plain-text menu instead of ncurses")
    parser.add_argument("--gui", action="store_true", help="use GUI window mode (PySide)")
    parser.add_argument("--clks-only", action="store_true", help="only expose CLKS options and emit CLKS-only config")
    parser.add_argument(
        "--preset",
        choices=["full", "minimal", "dev"],
        help="apply a built-in preset before interactive edit or save",
    )
    parser.add_argument(
        "--set",
        action="append",
        default=[],
        metavar="KEY=Y|M|N",
        help="override one option before save (can be repeated)",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    if args.gui and args.plain:
        raise RuntimeError("--gui and --plain cannot be used together")

    clks_options = load_clks_options()
    clks_only_mode = args.clks_only or not APPS_DIR.exists()
    if clks_only_mode and not args.clks_only:
        print(f"menuconfig: cleonos app directory not found, switching to CLKS-only mode ({APPS_DIR})")
    user_options = [] if clks_only_mode else discover_user_apps()
    all_options = clks_options + user_options

    previous = load_previous_values(include_user=not clks_only_mode)
    values = init_values(all_options, previous, use_defaults=args.defaults)

    if args.preset:
        apply_preset(args.preset, clks_options, user_options, values)

    option_index = _build_index(all_options)
    parse_set_overrides(values, option_index, args.set)

    should_save = args.non_interactive
    if not args.non_interactive:
        if args.gui:
            should_save = interactive_menu_gui(clks_options, user_options, values)
        else:
            if not sys.stdin.isatty():
                raise RuntimeError("menuconfig requires interactive tty (or use --non-interactive or --gui)")
            if args.plain:
                should_save = interactive_menu(clks_options, user_options, values)
            else:
                should_save = interactive_menu_ncurses(clks_options, user_options, values)

    if not should_save:
        print("menuconfig: no changes saved")
        return 0

    final_eval = evaluate_config(all_options, values)
    write_outputs(final_eval.effective, clks_options, user_options)
    print(f"menuconfig: wrote {CONFIG_JSON_PATH}")
    print(f"menuconfig: wrote {CONFIG_CMAKE_PATH}")
    print(f"menuconfig: wrote {CONFIG_CLKS_JSON_PATH}")
    print(f"menuconfig: wrote {CONFIG_CLKS_CMAKE_PATH}")
    if user_options:
        print(f"menuconfig: wrote {CONFIG_CLEONOS_JSON_PATH}")
        print(f"menuconfig: wrote {CONFIG_CLEONOS_CMAKE_PATH}")
    else:
        print("menuconfig: CLeonOS app config skipped (CLKS-only mode)")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as exc:
        print(f"menuconfig error: {exc}", file=sys.stderr)
        raise SystemExit(1)
