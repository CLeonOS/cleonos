# CLKS Kernel

[English](README.md) | [简体中文](README.zh-CN.md)

CLKS is the kernel component of the CLeonOS project.  
It includes architecture startup code, interrupt handling, memory management, scheduler, syscall layer, storage, TTY/console, and core runtime services.

## Status

CLKS can be built in kernel-only mode from the current mono-repo, but it is not yet a fully independent repository build system.

- Kernel-only mode is available.
- Userland/ISO targets are optional and can be disabled.
- Some build scripts are still shared at repository root (`cmake/`, `configs/`, `scripts/`).

## Directory Layout

```text
clks/
|- arch/          # Architecture-specific startup and low-level code
|- include/       # Public kernel headers
|- kernel/        # Core kernel subsystems
|- rust/          # Rust staticlib used by kernel
|- third_party/   # Embedded third-party sources used by kernel
|- CMakeLists.txt # Kernel build rules
|- Makefile       # Kernel-focused wrapper (delegates to root build)
```

## Build (Kernel-Only)

From repository root:

```bash
make kernel CLEONOS_ENABLE=OFF
```

or via CLKS wrapper:

```bash
make -C clks kernel
```

## Menuconfig (CLKS Scope)

```bash
make menuconfig-clks
```

or:

```bash
make -C clks menuconfig
```

This updates CLKS-focused config outputs under `configs/menuconfig/` (including `config.clks.cmake`).

## Notes for Future Split

To make CLKS a standalone repo, the next required step is moving shared build assets into `clks/` (or vendoring equivalents), especially:

- `cmake/` helper scripts (`log.cmake`, symbol generation, tool checks)
- boot config and image packaging pieces currently under `configs/`
- menuconfig launcher and feature metadata currently under `scripts/` and `configs/menuconfig/`

## License

Apache-2.0 (same as project root).
