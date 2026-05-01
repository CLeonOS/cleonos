# CLeonOS Stage8

## Stage Goal
- Add kernel driver abstraction and registration framework.
- Package standard drivers (serial, framebuffer, tty, disk, net, audio, input) as `/driver/*.elf`.
- Probe `/driver` directory ELF drivers from ramdisk, validate ELF metadata, register them, and start them as driver processes.
- Extend user app packaging to include driver ELF in `/driver`.
- Add dynamic driver control syscalls plus `drvctl list/load/unload/reload`.
- Add unified `/dev` fd access for framebuffer, input, network, disk, and tty devices.

## Acceptance Criteria
- Kernel boots and prints `CLEONOS STAGE8 START`.
- Driver framework logs `DRIVER MANAGER ONLINE`.
- Standard `/driver/*.elf` files are discovered, validated, spawned, and logged as `DRIVER ELF LOADED`.
- `drvctl list` shows standard and third-party ELF drivers.
- `devtest` reads `/dev/fb0`, `/dev/net0`, `/dev/disk0`, and `/dev/input/mouse`.
- System continues to scheduler/interrupt/syscall idle loop without panic.

## Build Targets
- `make setup`
- `make userapps`
- `make iso`
- `make run`
- `make debug`

## QEMU Command
- `qemu-system-x86_64 -M q35 -m 1024M -cdrom build/CLeonOS-x86_64.iso -serial stdio`

## Common Bugs and Debugging
- `DRIVER ELF INVALID`:
  - Verify ELF is built as x86_64 ELF64 and has valid program headers.
- `DRIVER ELF MISSING`:
  - Ensure `make userapps` finished and ramdisk staging copied files to `/driver`.
- `DRIVER ELF SPAWN FAILED`:
  - Confirm the driver ELF is also a normal CLeonOS user executable and can be launched by the exec runtime.
- `drvctl unload` fails:
  - Confirm the target driver is an active `/driver/*.elf` entry and the driver name/path matches `drvctl list`.
- Driver count is lower than expected:
  - Check `clks_driver_init()` call order occurs after `clks_fs_init()`.
- Build failure for `*drv_main.c` symbols:
  - Confirm the driver app is listed in the `project.bdt` `output_group.driver.apps` list and copied into ramdisk `/driver`.
- No driver logs on boot:
  - Confirm kernel includes `clks/kernel/runtime/driver.c` in the kernel source list.
