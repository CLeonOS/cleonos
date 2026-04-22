# CLeonOS

[English](README.md) | [简体中文](README.zh-CN.md)

Experimental x86_64 operating system project with a C kernel, Rust-assisted runtime pieces, user-space ELF apps, and a stage-based development history.

## Highlights

- x86_64 kernel booted by Limine
- RAM-disk VFS layout (`/system`, `/shell`, `/temp`, `/driver`)
- Virtual disk backend with FAT32 format + mount support (default mount path: `/temp/disk`)
- Virtual TTY subsystem (multi TTY, ANSI handling, cursor, PSF font support)
- Keyboard and mouse input stack, plus desktop mode on TTY2
- User-space ELF app model with syscall ABI (`int 0x80`)
- User shell with external command apps (`ls`, `cat`, `grep`, `mkdir`, `cp`, `mv`, `rm`, etc.)
- Pipe and redirection support in user shell (`|`, `>`, `>>`)
- Optional host-side CLeonOS-Wine runner (Python + Unicorn) in [`wine/`](wine)

## Repository Layout

```text
.
|- clks/                 # Kernel sources (arch, drivers, scheduler, tty, syscall, ...)
|- cleonos/              # Userland runtime, libc-like layer, user apps, Rust user library
|- ramdisk/              # Static files copied into runtime ramdisk
|- configs/              # Boot configuration (Limine)
|- cmake/                # Shared CMake scripts (tool checks, logging, limine setup)
|- docs/                 # Stage documents and syscall reference
|- wine/                 # Host runner for CLeonOS user ELF (no full VM required)
|- CMakeLists.txt        # Main build definition
|- Makefile              # Developer-friendly wrapper around CMake targets
```

## Build Requirements

Minimum required tools:

- `cmake` (>= 3.20)
- `make`
- `git`
- `tar`
- `xorriso`
- `sh` (POSIX shell)
- `rustc`
- Kernel/user toolchain (resolved automatically with fallback):
  - kernel: `x86_64-elf-gcc`/`x86_64-elf-ld` (or fallback `gcc`/`clang` + `ld.lld`)
  - user: `cc` + `ld`

For building Limine from source, install extras such as `autoconf`, `automake`, `libtool`, `pkg-config`, `mtools`, and `nasm`.

For runtime:

- `qemu-system-x86_64` (for `make run` / `make debug`)

## Quick Start

```bash
git clone <your-repo-url>
cd cleonos
git submodule update --init --recursive
make run
```

`make run` now auto-prepares `build/x86_64/cleonos_disk.img` (if missing) and attaches it as a QEMU emulated physical disk (`-drive ... if=ide`).
This disk is **not** packed into the ISO and is **not** loaded through Limine modules.
Kernel currently uses an in-memory disk cache window (default up to 8MB) for metadata/file operations.
You can override disk size (MB), for example: `make run DISK_IMAGE_MB=128`.
Inside CLeonOS, use `diskinfo` to confirm the disk is visible.

If you already have Limine artifacts and want to skip configure:

```bash
make run LIMINE_SKIP_CONFIGURE=1
```

## Common Targets

- `make setup` - check tools and prepare Limine
- `make kernel` - build kernel ELF
- `make userapps` - build user-space ELF apps
- `make ramdisk` - package runtime ramdisk
- `make disk-image` - create/resize runtime disk image (`build/x86_64/cleonos_disk.img`)
- `make iso` - build bootable ISO
- `make run` - launch QEMU
- `make debug` - launch QEMU with `-s -S` for GDB
- `make clean` - clean `build/x86_64`
- `make clean-all` - clean all build output

## Debugging (GDB)

Start debug VM:

```bash
make debug
```

Attach in another terminal:

```bash
gdb build/x86_64/clks_kernel.elf
(gdb) target remote :1234
```

## User Shell Examples

```sh
help
ls /shell
cat /shell/init.cmd
grep -n exec /shell/init.cmd
cat /shell/init.cmd | grep -n exec
ls /shell > /temp/shell_list.txt
diskinfo
mkfsfat32 CLEONOS
mount /temp/disk
write /temp/disk/hello.txt hello-disk
```

## Documentation

- Stage index: [`docs/README.md`](docs/README.md)
- Syscall ABI reference: [`docs/syscall.md`](docs/syscall.md)

## CI

GitHub Actions workflow [`build-os.yml`](.github/workflows/build-os.yml) builds the OS ISO on push and pull request, and uploads the ISO as an artifact.

## Contributing

1. Fork and create a feature branch.
2. Keep changes stage-oriented and update docs when behavior changes.
3. Run at least `make iso` before opening a PR.
4. Include boot log snippets or screenshots for kernel/user visible changes.

## License

Apache-2.0. See [`License`](License).
