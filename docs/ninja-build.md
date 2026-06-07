# Ninja Build

CLeonOS has a standalone Ninja build path. It coexists with bdt, but it does not call bdt and does not use bdt as a frontend.

## Design

- `scripts/gen_ninja.py` generates `build.ninja` in the repository root.
- `build.ninja` is generated output and is not committed.
- Ninja directly calls tools such as `gcc`, `g++`, `ld`, `rustc`, `xorriso`, `tar`, and `qemu-system-x86_64`.
- The generator has its own Ninja-specific build manifest and does not read `project.bdt`.
- Existing bdt targets still work, including `make iso`, `make userapps`, and `make run`.

## Usage

Run from WSL:

```sh
cd /mnt/d/Projects/C/cleonos
python3 scripts/gen_ninja.py
ninja -f build.ninja clboot-iso
```

Makefile shortcuts are also available:

```sh
make ninja-gen
make ninja-clboot-iso
make ninja-build
```

## Targets

List targets:

```sh
ninja -f build.ninja -t targets all
```

Common targets:

- `kernel`: build the Limine kernel ELF.
- `clboot`: build the CLBoot UEFI app `BOOTX64.EFI`.
- `clboot-kernel`: build the CLBoot protocol kernel ELF.
- `userapps`: build user-mode ELF programs.
- `ramdisk-root`: generate the ramdisk root directory.
- `ramdisk`: package the ramdisk tar archive.
- `clboot-iso-efi`: generate the FAT EFI boot image embedded in the CLBoot ISO. It contains `BOOTX64.EFI`, the CLBoot kernel ELF, and the ramdisk.
- `clboot-iso`: generate the CLBoot UEFI ISO.
- `disk-image`: create the QEMU test disk image.
- `run-clboot`: run CLBoot in QEMU with OVMF by booting the generated CLBoot ISO as a UEFI CD-ROM.
- `run`: alias for `run-clboot`.
- `all`: default target, currently equivalent to `clboot-iso`.

## WSL Validation

```sh
cd /mnt/d/Projects/C/cleonos
python3 -m py_compile scripts/gen_ninja.py
python3 scripts/gen_ninja.py
ninja -f build.ninja -t targets all | head -80
ninja -f build.ninja -n clboot
ninja -f build.ninja -n clboot-kernel
ninja -f build.ninja -n userapps | head -120
ninja -f build.ninja -n clboot-iso | head -160
ninja -f build.ninja -n run-clboot
```

## Notes

The Ninja path is an independent build entrypoint, not a bdt frontend. If an app rule is changed in bdt's `project.bdt`, update the Ninja-specific manifest in `scripts/gen_ninja.py` when the Ninja build should match that change.

`clboot-iso` creates `build/CLeonOS-CLBoot-x86_64.iso`. `run-clboot` boots that ISO directly as a UEFI CD-ROM and does not use QEMU `-kernel` or a standalone ESP disk. The ISO contains an El Torito FAT EFI image with CLBoot, the kernel ELF, and the ramdisk. `clboot-esp` still creates `build/x86_64/clboot_efi.img` as a standalone ESP disk image for debugging, but it is not used by `run-clboot`.
