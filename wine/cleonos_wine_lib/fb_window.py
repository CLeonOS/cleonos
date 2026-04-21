from __future__ import annotations

import sys
import time
from typing import Optional

from .state import SharedKernelState


CLEONOS_KEY_LEFT = 0x01
CLEONOS_KEY_RIGHT = 0x02
CLEONOS_KEY_UP = 0x03
CLEONOS_KEY_DOWN = 0x04


class FBWindow:
    def __init__(self, pygame_mod, width: int, height: int, scale: int, max_fps: int, *, verbose: bool = False) -> None:
        self._pygame = pygame_mod
        self.width = int(width)
        self.height = int(height)
        self.scale = max(1, int(scale))
        self._verbose = bool(verbose)
        self._closed = False
        self._last_present_ns = 0
        self._frame_interval_ns = int(1_000_000_000 / max_fps) if int(max_fps) > 0 else 0

        window_w = self.width * self.scale
        window_h = self.height * self.scale

        self._pygame.display.set_caption("CLeonOS Wine Framebuffer")
        self._screen = self._pygame.display.set_mode((window_w, window_h))

    @classmethod
    def create(
        cls,
        width: int,
        height: int,
        scale: int,
        max_fps: int,
        *,
        verbose: bool = False,
    ) -> Optional["FBWindow"]:
        try:
            import pygame  # type: ignore
        except Exception as exc:
            print(f"[WINE][WARN] framebuffer window disabled: pygame import failed ({exc})", file=sys.stderr)
            return None

        try:
            pygame.init()
            pygame.display.init()
            return cls(pygame, width, height, scale, max_fps, verbose=verbose)
        except Exception as exc:
            print(f"[WINE][WARN] framebuffer window disabled: unable to init display ({exc})", file=sys.stderr)
            try:
                pygame.quit()
            except Exception:
                pass
            return None

    def close(self) -> None:
        if self._closed:
            return

        self._closed = True
        try:
            self._pygame.display.quit()
        except Exception:
            pass
        try:
            self._pygame.quit()
        except Exception:
            pass

    def is_closed(self) -> bool:
        return self._closed

    def pump_input(self, state: SharedKernelState) -> None:
        if self._closed:
            return

        try:
            events = self._pygame.event.get()
        except Exception:
            self.close()
            return

        for event in events:
            if event.type == self._pygame.QUIT:
                self.close()
                continue

            if event.type != self._pygame.KEYDOWN:
                continue

            key = event.key
            ch = 0

            if key == self._pygame.K_LEFT:
                ch = CLEONOS_KEY_LEFT
            elif key == self._pygame.K_RIGHT:
                ch = CLEONOS_KEY_RIGHT
            elif key == self._pygame.K_UP:
                ch = CLEONOS_KEY_UP
            elif key == self._pygame.K_DOWN:
                ch = CLEONOS_KEY_DOWN
            elif key in (self._pygame.K_RETURN, self._pygame.K_KP_ENTER):
                ch = ord("\n")
            elif key == self._pygame.K_BACKSPACE:
                ch = 8
            elif key == self._pygame.K_ESCAPE:
                ch = 27
            elif key == self._pygame.K_TAB:
                ch = ord("\t")
            else:
                text = getattr(event, "unicode", "")
                if isinstance(text, str) and len(text) == 1:
                    code = ord(text)
                    if 32 <= code <= 126:
                        ch = code

            if ch != 0:
                state.push_key(ch)

    def present(self, pixels_bgra: bytearray, *, force: bool = False) -> bool:
        if self._closed:
            return False

        now_ns = time.monotonic_ns()
        if not force and self._frame_interval_ns > 0 and (now_ns - self._last_present_ns) < self._frame_interval_ns:
            return False

        try:
            src = self._pygame.image.frombuffer(pixels_bgra, (self.width, self.height), "BGRA")
            if self.scale != 1:
                src = self._pygame.transform.scale(src, (self.width * self.scale, self.height * self.scale))
            self._screen.blit(src, (0, 0))
            self._pygame.display.flip()
            self._last_present_ns = now_ns
            return True
        except Exception:
            self.close()
            return False
