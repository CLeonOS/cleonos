# Stage 30 - User-Space Window Manager (UWM) + Mouse Syscall

## Goal
- Add a minimal user-space desktop environment entrypoint.
- Provide a simple window manager demo (`uwm`) with focus, dragging, and rendering in framebuffer.
- Expose mouse snapshot data to user-space through a dedicated syscall.

## Implementation
- New syscall:
  - Added `MOUSE_STATE` (`id=107`) in kernel/user syscall headers.
  - Kernel side (`clks/kernel/runtime/syscall.c`) now exports mouse snapshot:
    - fields: `x`, `y`, `buttons`, `packet_count`, `ready`.
  - User wrapper added: `cleonos_sys_mouse_state()`.
- New user app:
  - Added `cleonos/c/apps/uwm_main.c`.
  - Features:
    - Desktop background rendering.
    - Multiple windows with z-order.
    - Active window focus + border highlight.
    - Drag by title bar (left mouse).
    - Keyboard fallback controls (`Tab`, `W/A/S/D`, `Q/Esc`).
- Build integration:
  - Added `uwm` into shell command app set in `cleonos/CMakeLists.txt`.
  - Updated shell help text and standalone `help` app output.
- QEMU run/debug update:
  - Added optional absolute pointer device by default:
    - `-usb -device usb-tablet`
  - New CMake option:
    - `CLEONOS_QEMU_ENABLE_USB_TABLET=ON` (default).

## Acceptance Criteria
- `uwm` can be started from user shell.
- Framebuffer window demo draws multiple windows and supports dragging.
- Mouse state is retrievable from user-space via syscall `107`.
- `make run`/`make debug` use USB tablet by default for smoother pointer interaction.

## Build Targets
- `make userapps`
- `make iso`
- `make run`

## QEMU Command
- `make run`

## Debug Notes
- If `uwm` reports framebuffer unavailable:
  - verify `FB_INFO` syscall is enabled and framebuffer bpp is `32`.
- If pointer behavior is relative/jumpy:
  - keep `CLEONOS_QEMU_ENABLE_USB_TABLET=ON`.
  - for manual QEMU invocation, include `-usb -device usb-tablet`.
- If mouse syscall returns `ready=0`:
  - check boot log for PS/2 mouse init status under `[INFO][MOUSE]`.
