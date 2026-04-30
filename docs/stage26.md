# Stage 26 - Kernel Builtin ELF Loader + Root Hello ELF

## Goal
- Move `elfloader` from standalone user ELF into a kernel builtin command.
- Keep `hello.elf` as root-level user ELF test payload.

## Implementation
- Removed standalone user app `elfloader_main.c`.
- Added kernel shell builtin command: `elfloader [path]`.
  - Default target path: `/hello.elf`.
  - Builtin flow: `fs_read -> elf64 inspect -> exec load -> entry call -> return status`.
- Updated ramdisk placement rules:
  - `hello.elf` -> `/hello.elf`
- Simplified user Rust library back to shared helper export (`cleonos_rust_guarded_len`).
- Added x86_64 exec stack bridge (`clks_exec_call_on_stack_x86_64`) so ELF entry runs on a dedicated execution stack instead of inheriting deep shell call stack.

## Acceptance Criteria
- No `elfloader.elf` is generated or packed.
- Ramdisk root contains `/hello.elf`.
- In kernel shell:
  - `elfloader` loads and executes `/hello.elf`, then returns status.
  - `elfloader /path/to/app.elf` works for other absolute/relative paths.
- `exec /shell/shell.elf` can take foreground control without immediate stack-chain crash.

## Build Targets
- `make userapps`
- `make ramdisk`
- `make iso`
- `make run`

## QEMU Command
- `make run`

## Debug Notes
- If `elfloader` reports `file missing`, check ramdisk root packaging for `/hello.elf`.
- If it reports `invalid elf64`, verify user app link script and ELF output format.
- If it reports `exec failed`, inspect `EXEC` channel logs for load/entry/return status.
- For long-running interactive ELF (such as `shell.elf`), no `RUN RETURNED` log is expected unless the app exits.
