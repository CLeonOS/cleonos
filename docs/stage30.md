# Stage 30 - User-Space Window Manager (UWM) + Mouse Syscall

## Goal
- Add a minimal user-space desktop environment entrypoint.
- Provide a usable user-space window manager (`uwm`) with focus, dragging, resize, taskbar, and launcher.
- Expose mouse snapshot data to user-space through a dedicated syscall.

## Implementation
- New syscall:
  - Added `MOUSE_STATE` (`id=107`) in kernel/user syscall headers.
  - Kernel side (`clks/kernel/runtime/syscall.c`) now exports mouse snapshot:
    - fields: `x`, `y`, `buttons`, `packet_count`, `ready`.
  - User wrapper added: `cleonos_sys_mouse_state()`.
- Window protocol update:
  - Added `WM_SET_FLAGS` (`id=114`) for `CLEONOS_WM_FLAG_TOPMOST`.
  - Added `WM_RESIZE` (`id=115`) so resize keeps the same `window_id`.
- New user app:
  - Added `cleonos/c/apps/uwm_main.c`.
  - Features:
    - Bottom taskbar with window buttons.
    - Start launcher menu for built-in demo windows.
    - Multiple windows with z-order.
    - Title bar with close, minimize, topmost, and resize controls.
    - Active window focus + border highlight.
    - Drag by title bar (left mouse).
    - Keyboard fallback controls (`Tab`, `1/2/3`, `W/A/S/D`, `M`, `X`, `T`, `+/-`, `Q`).
- Build integration:
  - Added `uwm` into shell command app set.
  - Updated shell help text and standalone `help` app output.
- QEMU run/debug update:
  - QEMU now keeps the default PS/2 mouse path enabled for `make run`/`make debug`.
  - USB tablet is optional and disabled by default because the kernel currently has a PS/2 mouse driver, not a USB HID tablet driver.
  - Optional absolute pointer device:
    - `-usb -device usb-tablet`
  - New build option:
    - `cleonos_qemu_enable_usb_tablet = OFF` (default).
    - Set it to `ON` only after USB HID tablet support is available.

## Acceptance Criteria
- `uwm` can be started from user shell.
- UWM draws multiple windows, taskbar, and launcher, and supports dragging, minimize/restore, topmost, close, and resize.
- Mouse state is retrievable from user-space via syscall `107`.
- `make run`/`make debug` use the PS/2 mouse path by default so the existing kernel mouse driver receives movement and buttons.

## Build Targets
- `make userapps`
- `make iso`
- `make run`

## QEMU Command
- `make run`

## Debug Notes
- If `uwm` reports framebuffer unavailable:
  - verify `FB_INFO` syscall is enabled and framebuffer bpp is `32`.
- If the UWM cursor does not move under QEMU:
  - keep `CLEONOS_QEMU_ENABLE_USB_TABLET=OFF`.
  - for manual QEMU invocation, do not include `-usb -device usb-tablet` until USB HID support exists.
- If mouse syscall returns `ready=0`:
  - check boot log for PS/2 mouse init status under `[INFO][MOUSE]`.
