# CLeonOS App Kit

[English](README.md) | [简体中文](README.zh-CN.md)

`kit/` is a standalone user-app SDK for CLeonOS.
It builds ring3 ELF apps **without** depending on CLKS kernel source.

## Toolchain

Use an **ELF** toolchain (not MinGW PE/COFF):

- Recommended: `x86_64-elf-gcc` + `x86_64-elf-ld`
- Alternative: `clang` + ELF-mode `ld.lld`

## What Is Included

- User runtime (`runtime/`) copied from CLeonOS userland
- Syscall/libc-like headers (`include/`)
- User linker script (`linker/user.ld`)
- CMake project and Makefile wrapper
- Sample app: `apps/hello/main.c`

Default sample behavior:
- prints `Hello world!`

## Quick Start

```bash
cd kit
make apps
```

Output ELF files are generated under:

```text
kit/build/apps/*.elf
```

## Build Only Specific Apps

Build one app:

```bash
make apps APPS=hello
```

Build multiple apps:

```bash
make apps APPS=hello,foo,bar
```

Each app must be a directory under `apps/<name>/` with one or more `.c` files.

## App Entry

By default, kit enables `runtime/main_adapter.c`, so your app can define standard:

```c
int main(int argc, char **argv)
```

If you need raw CLeonOS entry (`cleonos_app_main(int,char**,char**)`), configure with:

```bash
cmake -S . -B build-cmake -DCLEONOS_KIT_USE_MAIN_ADAPTER=OFF
```

## CMake Direct Usage

```bash
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake --target apps
```

Explicit ELF toolchain example:

```bash
cmake -S . -B build-cmake \
  -DCLEONOS_KIT_CC=x86_64-elf-gcc \
  -DCLEONOS_KIT_LD=x86_64-elf-ld
cmake --build build-cmake --target apps
```

Useful options:

- `-DCLEONOS_KIT_APP_NAMES=hello,foo`
- `-DCLEONOS_KIT_ENABLE_WERROR=OFF`
- `-DCLEONOS_KIT_OUTPUT_DIR=<path>`

## Add New App

1. Create directory: `apps/myapp/`
2. Add source file(s), for example `apps/myapp/main.c`
3. Rebuild with `make apps`

Kit auto-discovers app directories.
