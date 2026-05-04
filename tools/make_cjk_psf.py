#!/usr/bin/env python3
"""Generate CLeonOS TTY PSF2 font with direct Unicode codepoint indexing.

The generated PSF2 has glyph index == Unicode codepoint for U+0000..U+FFFD.
That keeps kernel glyph lookup O(1) for ASCII, CJK, box drawing and fullwidth
punctuation without a large runtime unicode-map parser dependency.
"""

import ctypes
import ctypes.util
import os
import struct
import subprocess
import sys

WIDTH = 24
CELL_WIDTH = 12
HEIGHT = 24
BASELINE = 20
MAX_CODEPOINT = 0xFFFD
BYTES_PER_ROW = (WIDTH + 7) // 8
CHAR_SIZE = BYTES_PER_ROW * HEIGHT
PSF2_MAGIC = 0x864AB572

# Keep rendering bounded to useful BMP ranges. Other entries get '?' fallback.
RENDER_RANGES = [
    (0x0020, 0x007E),  # ASCII
    (0x00A0, 0x024F),  # Latin supplement/extended
    (0x0370, 0x03FF),  # Greek
    (0x0400, 0x04FF),  # Cyrillic
    (0x2000, 0x206F),  # General punctuation
    (0x2190, 0x21FF),  # Arrows
    (0x2500, 0x257F),  # Box drawing
    (0x2580, 0x259F),  # Block elements
    (0x25A0, 0x25FF),  # Geometric shapes
    (0x3000, 0x303F),  # CJK symbols/punctuation
    (0x3040, 0x30FF),  # Hiragana/Katakana
    (0x3100, 0x312F),  # Bopomofo
    (0x31F0, 0x31FF),  # Katakana phonetic extensions
    (0x3400, 0x4DBF),  # CJK Extension A
    (0x4E00, 0x9FFF),  # CJK Unified Ideographs
    (0xF900, 0xFAFF),  # CJK compatibility ideographs
    (0xFE10, 0xFE6F),  # Vertical/small forms
    (0xFF00, 0xFFEF),  # Fullwidth forms
]

FT_LOAD_RENDER = 0x4
FT_LOAD_TARGET_MONO = 0x20000


def find_font():
    if os.environ.get("CJK_FONT"):
        return (os.environ["CJK_FONT"], int(os.environ.get("CJK_FONT_INDEX", "0")))
    candidates = ["Noto Sans Mono CJK SC", "Noto Sans CJK SC", "Droid Sans Fallback"]
    for name in candidates:
        try:
            out = subprocess.check_output(["fc-match", "-f", "%{file}\t%{index}\n", name], text=True).strip()
            if not out:
                continue
            parts = out.splitlines()[0].split("\t")
            path = parts[0]
            index = int(parts[1]) if len(parts) > 1 and parts[1] else 0
            if path and os.path.exists(path):
                return (path, index)
        except Exception:
            pass
    raise SystemExit("No CJK font found. Install Noto Sans CJK or set CJK_FONT=/path/to/font.ttf")


class FT_Bitmap(ctypes.Structure):
    _fields_ = [
        ("rows", ctypes.c_uint),
        ("width", ctypes.c_uint),
        ("pitch", ctypes.c_int),
        ("buffer", ctypes.POINTER(ctypes.c_ubyte)),
        ("num_grays", ctypes.c_ushort),
        ("pixel_mode", ctypes.c_ubyte),
        ("palette_mode", ctypes.c_ubyte),
        ("palette", ctypes.c_void_p),
    ]


class FT_Generic(ctypes.Structure):
    _fields_ = [
        ("data", ctypes.c_void_p),
        ("finalizer", ctypes.c_void_p),
    ]


class FT_Vector(ctypes.Structure):
    _fields_ = [
        ("x", ctypes.c_long),
        ("y", ctypes.c_long),
    ]


class FT_BBox(ctypes.Structure):
    _fields_ = [
        ("xMin", ctypes.c_long),
        ("yMin", ctypes.c_long),
        ("xMax", ctypes.c_long),
        ("yMax", ctypes.c_long),
    ]


class FT_Glyph_Metrics(ctypes.Structure):
    _fields_ = [
        ("width", ctypes.c_long),
        ("height", ctypes.c_long),
        ("horiBearingX", ctypes.c_long),
        ("horiBearingY", ctypes.c_long),
        ("horiAdvance", ctypes.c_long),
        ("vertBearingX", ctypes.c_long),
        ("vertBearingY", ctypes.c_long),
        ("vertAdvance", ctypes.c_long),
    ]


class FT_Outline(ctypes.Structure):
    _fields_ = [
        ("n_contours", ctypes.c_short),
        ("n_points", ctypes.c_short),
        ("points", ctypes.POINTER(FT_Vector)),
        ("tags", ctypes.c_char_p),
        ("contours", ctypes.POINTER(ctypes.c_short)),
        ("flags", ctypes.c_int),
    ]


class FT_GlyphSlotRec(ctypes.Structure):
    _fields_ = [
        ("library", ctypes.c_void_p),
        ("face", ctypes.c_void_p),
        ("next", ctypes.c_void_p),
        ("glyph_index", ctypes.c_uint),
        ("generic", FT_Generic),
        ("metrics", FT_Glyph_Metrics),
        ("linearHoriAdvance", ctypes.c_long),
        ("linearVertAdvance", ctypes.c_long),
        ("advance", FT_Vector),
        ("format", ctypes.c_uint),
        ("bitmap", FT_Bitmap),
        ("bitmap_left", ctypes.c_int),
        ("bitmap_top", ctypes.c_int),
        ("outline", FT_Outline),
    ]


class FT_FaceRec(ctypes.Structure):
    _fields_ = [
        ("num_faces", ctypes.c_long),
        ("face_index", ctypes.c_long),
        ("face_flags", ctypes.c_long),
        ("style_flags", ctypes.c_long),
        ("num_glyphs", ctypes.c_long),
        ("family_name", ctypes.c_char_p),
        ("style_name", ctypes.c_char_p),
        ("num_fixed_sizes", ctypes.c_int),
        ("available_sizes", ctypes.c_void_p),
        ("num_charmaps", ctypes.c_int),
        ("charmaps", ctypes.c_void_p),
        ("generic", FT_Generic),
        ("bbox", FT_BBox),
        ("units_per_EM", ctypes.c_ushort),
        ("ascender", ctypes.c_short),
        ("descender", ctypes.c_short),
        ("height", ctypes.c_short),
        ("max_advance_width", ctypes.c_short),
        ("max_advance_height", ctypes.c_short),
        ("underline_position", ctypes.c_short),
        ("underline_thickness", ctypes.c_short),
        ("glyph", ctypes.POINTER(FT_GlyphSlotRec)),
    ]


def pack_bitmap(canvas):
    out = bytearray()
    for y in range(HEIGHT):
        value = 0
        bit = 0
        for x in range(WIDTH):
            if canvas[y][x]:
                value |= 0x80 >> bit
            bit += 1
            if bit == 8:
                out.append(value)
                value = 0
                bit = 0
        if bit:
            out.append(value)
    return bytes(out)


def codepoint_width(codepoint):
    if codepoint == 0:
        return 0
    if codepoint < 0x1100:
        return 1
    if (
        0x1100 <= codepoint <= 0x115F
        or codepoint in (0x2329, 0x232A)
        or 0x2E80 <= codepoint <= 0xA4CF
        or 0xAC00 <= codepoint <= 0xD7A3
        or 0xF900 <= codepoint <= 0xFAFF
        or 0xFE10 <= codepoint <= 0xFE19
        or 0xFE30 <= codepoint <= 0xFE6F
        or 0xFF00 <= codepoint <= 0xFF60
        or 0xFFE0 <= codepoint <= 0xFFE6
        or 0x20000 <= codepoint <= 0x3FFFD
    ):
        return 2
    return 1


def render_codepoint(ft, face, codepoint):
    err = ft.FT_Load_Char(face, ctypes.c_ulong(codepoint), ctypes.c_int(FT_LOAD_RENDER | FT_LOAD_TARGET_MONO))
    if err != 0:
        err = ft.FT_Load_Char(face, ctypes.c_ulong(codepoint), ctypes.c_int(FT_LOAD_RENDER))
        if err != 0:
            return None

    rec = ctypes.cast(face, ctypes.POINTER(FT_FaceRec)).contents
    slot = rec.glyph.contents
    bitmap = slot.bitmap
    if not bitmap.buffer:
        return bytes(CHAR_SIZE)

    canvas = [[0] * WIDTH for _ in range(HEIGHT)]
    glyph_w = int(bitmap.width)
    glyph_h = int(bitmap.rows)
    pitch = abs(int(bitmap.pitch))
    target_width = WIDTH if codepoint_width(codepoint) > 1 else CELL_WIDTH
    xoff = (target_width - glyph_w) // 2 + min(int(slot.bitmap_left), 1)
    if xoff < 0:
        xoff = 0
    yoff = BASELINE - int(slot.bitmap_top)
    if yoff < 0:
        yoff = 0

    base_addr = ctypes.addressof(bitmap.buffer.contents)
    if bitmap.pixel_mode == 1:  # FT_PIXEL_MODE_MONO
        for y in range(glyph_h):
            yy = yoff + y
            if yy < 0 or yy >= HEIGHT:
                continue
            row = ctypes.cast(base_addr + y * pitch, ctypes.POINTER(ctypes.c_ubyte))
            for x in range(glyph_w):
                xx = xoff + x
                if 0 <= xx < WIDTH and (row[x >> 3] & (0x80 >> (x & 7))):
                    canvas[yy][xx] = 1
    else:
        for y in range(glyph_h):
            yy = yoff + y
            if yy < 0 or yy >= HEIGHT:
                continue
            row = ctypes.cast(base_addr + y * pitch, ctypes.POINTER(ctypes.c_ubyte))
            for x in range(glyph_w):
                xx = xoff + x
                if 0 <= xx < WIDTH and row[x] >= 96:
                    canvas[yy][xx] = 1

    return pack_bitmap(canvas)


def should_render(cp):
    return any(start <= cp <= end for start, end in RENDER_RANGES)


def main(argv):
    font_path, font_index = find_font()
    ft_path = ctypes.util.find_library("freetype")
    if not ft_path:
        raise SystemExit("libfreetype not found")
    ft = ctypes.CDLL(ft_path)

    lib = ctypes.c_void_p()
    face = ctypes.c_void_p()
    if ft.FT_Init_FreeType(ctypes.byref(lib)) != 0:
        raise SystemExit("FT_Init_FreeType failed")
    if ft.FT_New_Face(lib, font_path.encode(), font_index, ctypes.byref(face)) != 0:
        raise SystemExit("FT_New_Face failed: " + font_path)
    if ft.FT_Set_Pixel_Sizes(face, WIDTH, HEIGHT) != 0:
        raise SystemExit("FT_Set_Pixel_Sizes failed")

    question = render_codepoint(ft, face, ord("?")) or bytes(CHAR_SIZE)
    blank = bytes(CHAR_SIZE)
    glyph_count = MAX_CODEPOINT + 1
    glyphs = bytearray(question * glyph_count)
    glyphs[ord(" ") * CHAR_SIZE:(ord(" ") + 1) * CHAR_SIZE] = blank

    rendered = 0
    for cp in range(glyph_count):
        if cp == ord(" ") or not should_render(cp):
            continue
        glyph = render_codepoint(ft, face, cp)
        if glyph is None:
            continue
        glyphs[cp * CHAR_SIZE:(cp + 1) * CHAR_SIZE] = glyph
        rendered += 1

    header = struct.pack("<IIIIIIII", PSF2_MAGIC, 0, 32, 0, glyph_count, CHAR_SIZE, HEIGHT, WIDTH)
    data = header + bytes(glyphs)

    outputs = argv[1:] or ["ramdisk/system/tty.psf"]
    for path in outputs:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(data)
        print(f"{path}: {glyph_count} glyphs, rendered={rendered}, bytes={len(data)}, font={font_path}, index={font_index}")

    ft.FT_Done_Face(face)
    ft.FT_Done_FreeType(lib)


if __name__ == "__main__":
    main(sys.argv)
