#define CLBOOT_GFX_FONT_PATH L"\\EFI\\CLEONOS\\clboot.psf"
#define CLBOOT_GFX_FONT_FALLBACK_PATH L"\\boot\\clboot.psf"
#define CLBOOT_GFX_PSF2_MAGIC 0x864AB572U

struct clboot_gfx_psf2_header {
    UINT32 magic;
    UINT32 version;
    UINT32 headersize;
    UINT32 flags;
    UINT32 length;
    UINT32 charsize;
    UINT32 height;
    UINT32 width;
};

struct clboot_gfx_font {
    UINT32 width;
    UINT32 height;
    UINT32 glyph_count;
    UINT32 bytes_per_glyph;
    UINT32 bytes_per_row;
    const UINT8 *glyphs;
};

struct clboot_gfx_state {
    int ready;
    UINT8 *fb;
    UINT32 width;
    UINT32 height;
    UINT32 pitch_pixels;
    UINT32 pixel_format;
    struct clboot_loaded_file font_file;
    struct clboot_gfx_font font;
};

static struct clboot_gfx_state clboot_gfx;

#define CLBOOT_GFX_CONSOLE_MAX_COLS 120U
#define CLBOOT_GFX_CONSOLE_MAX_ROWS 40U

struct clboot_gfx_console_state {
    int active;
    int cursor_visible;
    UINT32 panel_x;
    UINT32 panel_y;
    UINT32 panel_w;
    UINT32 panel_h;
    UINT32 body_x;
    UINT32 body_y;
    UINT32 body_w;
    UINT32 body_h;
    UINT32 cell_w;
    UINT32 cell_h;
    UINT32 cols;
    UINT32 rows;
    UINT32 cursor_col;
    UINT32 cursor_row;
    UINT32 fg;
    UINT32 bg;
    UINT32 body_bg;
    UINT32 accent;
    char cells[CLBOOT_GFX_CONSOLE_MAX_ROWS][CLBOOT_GFX_CONSOLE_MAX_COLS];
    UINT32 cell_fg[CLBOOT_GFX_CONSOLE_MAX_ROWS][CLBOOT_GFX_CONSOLE_MAX_COLS];
    UINT32 cell_bg[CLBOOT_GFX_CONSOLE_MAX_ROWS][CLBOOT_GFX_CONSOLE_MAX_COLS];
};

static struct clboot_gfx_console_state clboot_gfx_console;

static UINT32 clboot_gfx_rgb(UINT8 r, UINT8 g, UINT8 b) {
    if (clboot_gfx.pixel_format == 0U) {
        return ((UINT32)r) | ((UINT32)g << 8U) | ((UINT32)b << 16U);
    }
    return ((UINT32)b) | ((UINT32)g << 8U) | ((UINT32)r << 16U);
}

static int clboot_gfx_parse_psf2(const void *blob, uint64_t size, struct clboot_gfx_font *out_font) {
    const struct clboot_gfx_psf2_header *hdr;
    UINT32 bytes_per_row;
    uint64_t glyph_bytes;

    if (blob == (const void *)0 || out_font == (struct clboot_gfx_font *)0 ||
        size < (uint64_t)sizeof(struct clboot_gfx_psf2_header)) {
        return 0;
    }

    hdr = (const struct clboot_gfx_psf2_header *)blob;
    if (hdr->magic != CLBOOT_GFX_PSF2_MAGIC || hdr->headersize < sizeof(struct clboot_gfx_psf2_header) ||
        hdr->headersize > size || hdr->length == 0U || hdr->charsize == 0U || hdr->width == 0U ||
        hdr->height == 0U) {
        return 0;
    }

    bytes_per_row = (hdr->width + 7U) >> 3U;
    if ((uint64_t)bytes_per_row * (uint64_t)hdr->height > (uint64_t)hdr->charsize) {
        return 0;
    }

    glyph_bytes = (uint64_t)hdr->length * (uint64_t)hdr->charsize;
    if ((uint64_t)hdr->headersize + glyph_bytes > size) {
        return 0;
    }

    out_font->width = hdr->width;
    out_font->height = hdr->height;
    out_font->glyph_count = hdr->length;
    out_font->bytes_per_glyph = hdr->charsize;
    out_font->bytes_per_row = bytes_per_row;
    out_font->glyphs = ((const UINT8 *)blob) + hdr->headersize;
    return 1;
}

static int clboot_gfx_load_font(void) {
    CHAR16 *paths[] = {CLBOOT_GFX_FONT_PATH, CLBOOT_GFX_FONT_FALLBACK_PATH, (CHAR16 *)0};

    clboot_gfx.font_file.data = (void *)0;
    clboot_gfx.font_file.size = 0ULL;
    if (clboot_read_file_any(paths, &clboot_gfx.font_file) == 0) {
        return 0;
    }

    if (clboot_gfx_parse_psf2(clboot_gfx.font_file.data, clboot_gfx.font_file.size, &clboot_gfx.font) == 0) {
        return 0;
    }

    return 1;
}

static int clboot_gfx_init(void) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = (EFI_GRAPHICS_OUTPUT_PROTOCOL *)0;

    clboot_memset(&clboot_gfx, 0, sizeof(clboot_gfx));
    if (clboot_bs == (EFI_BOOT_SERVICES *)0 ||
        clboot_bs->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, (void *)0, (void **)&gop) != EFI_SUCCESS ||
        gop == (EFI_GRAPHICS_OUTPUT_PROTOCOL *)0 || gop->Mode == (EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *)0 ||
        gop->Mode->Info == (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *)0 || gop->Mode->FrameBufferBase == 0ULL) {
        clboot_log_append_ascii("GFX unavailable: GOP missing\n");
        return 0;
    }

    clboot_gfx.fb = (UINT8 *)(UINTN)gop->Mode->FrameBufferBase;
    clboot_gfx.width = gop->Mode->Info->HorizontalResolution;
    clboot_gfx.height = gop->Mode->Info->VerticalResolution;
    clboot_gfx.pitch_pixels = gop->Mode->Info->PixelsPerScanLine;
    clboot_gfx.pixel_format = gop->Mode->Info->PixelFormat;

    if (clboot_gfx_load_font() == 0) {
        clboot_log_append_ascii("GFX unavailable: PSF2 font missing\n");
        return 0;
    }

    clboot_gfx.ready = 1;
    clboot_log_append_ascii("GFX menu online\n");
    return 1;
}

static int clboot_gfx_available(void) {
    return clboot_gfx.ready;
}

static void clboot_gfx_put_pixel(UINT32 x, UINT32 y, UINT32 color) {
    UINT32 *row;

    if (x >= clboot_gfx.width || y >= clboot_gfx.height || clboot_gfx.fb == (UINT8 *)0) {
        return;
    }

    row = (UINT32 *)(void *)(clboot_gfx.fb + ((uint64_t)y * (uint64_t)clboot_gfx.pitch_pixels * 4ULL));
    row[x] = color;
}

static void clboot_gfx_fill_rect(UINT32 x, UINT32 y, UINT32 w, UINT32 h, UINT32 color) {
    UINT32 yy;
    UINT32 xx;

    if (x >= clboot_gfx.width || y >= clboot_gfx.height) {
        return;
    }
    if (x + w > clboot_gfx.width) {
        w = clboot_gfx.width - x;
    }
    if (y + h > clboot_gfx.height) {
        h = clboot_gfx.height - y;
    }

    for (yy = 0U; yy < h; yy++) {
        for (xx = 0U; xx < w; xx++) {
            clboot_gfx_put_pixel(x + xx, y + yy, color);
        }
    }
}

static void clboot_gfx_rect(UINT32 x, UINT32 y, UINT32 w, UINT32 h, UINT32 color) {
    if (w == 0U || h == 0U) {
        return;
    }
    clboot_gfx_fill_rect(x, y, w, 1U, color);
    clboot_gfx_fill_rect(x, y + h - 1U, w, 1U, color);
    clboot_gfx_fill_rect(x, y, 1U, h, color);
    clboot_gfx_fill_rect(x + w - 1U, y, 1U, h, color);
}

static void clboot_gfx_clear_rgb(UINT8 top_r, UINT8 top_g, UINT8 top_b, UINT8 bottom_r, UINT8 bottom_g,
                                 UINT8 bottom_b) {
    UINT32 y;

    for (y = 0U; y < clboot_gfx.height; y++) {
        UINT8 r = (UINT8)(((UINT32)top_r * (clboot_gfx.height - y) + (UINT32)bottom_r * y) / clboot_gfx.height);
        UINT8 g = (UINT8)(((UINT32)top_g * (clboot_gfx.height - y) + (UINT32)bottom_g * y) / clboot_gfx.height);
        UINT8 b = (UINT8)(((UINT32)top_b * (clboot_gfx.height - y) + (UINT32)bottom_b * y) / clboot_gfx.height);
        clboot_gfx_fill_rect(0U, y, clboot_gfx.width, 1U, clboot_gfx_rgb(r, g, b));
    }
}

static void clboot_gfx_draw_char(UINT32 x, UINT32 y, char ch, UINT32 color, UINT32 scale) {
    const UINT8 *glyph;
    UINT32 row;
    UINT32 col;
    UINT32 code = (UINT32)(unsigned char)ch;

    if (scale == 0U) {
        scale = 1U;
    }
    if (code >= clboot_gfx.font.glyph_count) {
        code = (UINT32)'?';
    }
    glyph = clboot_gfx.font.glyphs + ((uint64_t)code * (uint64_t)clboot_gfx.font.bytes_per_glyph);

    for (row = 0U; row < clboot_gfx.font.height; row++) {
        const UINT8 *row_bits = glyph + ((uint64_t)row * (uint64_t)clboot_gfx.font.bytes_per_row);
        for (col = 0U; col < clboot_gfx.font.width; col++) {
            UINT8 byte = row_bits[col >> 3U];
            UINT8 mask = (UINT8)(0x80U >> (col & 7U));
            if ((byte & mask) != 0U) {
                clboot_gfx_fill_rect(x + (col * scale), y + (row * scale), scale, scale, color);
            }
        }
    }
}

static void clboot_gfx_draw_text(UINT32 x, UINT32 y, const char *text, UINT32 color, UINT32 scale) {
    UINT32 cursor_x = x;
    UINT32 cursor_y = y;
    UINTN i = 0U;
    UINT32 step = (clboot_gfx.font.width + 1U) * (scale == 0U ? 1U : scale);

    if (text == (const char *)0) {
        return;
    }

    while (text[i] != '\0') {
        if (text[i] == '\n') {
            cursor_x = x;
            cursor_y += (clboot_gfx.font.height + 4U) * (scale == 0U ? 1U : scale);
        } else if (text[i] == '\r') {
            cursor_x = x;
        } else {
            clboot_gfx_draw_char(cursor_x, cursor_y, text[i], color, scale);
            cursor_x += step;
        }
        i++;
    }
}

static void clboot_gfx_draw_text_wide(UINT32 x, UINT32 y, const CHAR16 *text, UINT32 color, UINT32 scale) {
    UINT32 cursor_x = x;
    UINTN i = 0U;
    UINT32 step = (clboot_gfx.font.width + 1U) * (scale == 0U ? 1U : scale);

    if (text == (const CHAR16 *)0) {
        return;
    }

    while (text[i] != 0U) {
        char ch = (text[i] < 128U) ? (char)text[i] : '?';
        clboot_gfx_draw_char(cursor_x, y, ch, color, scale);
        cursor_x += step;
        i++;
    }
}

static int clboot_gfx_console_active(void) {
    return (clboot_gfx.ready != 0 && clboot_gfx_console.active != 0) ? 1 : 0;
}

static void clboot_gfx_console_draw_cell(UINT32 row, UINT32 col) {
    UINT32 x;
    UINT32 y;
    char ch;

    if (row >= clboot_gfx_console.rows || col >= clboot_gfx_console.cols) {
        return;
    }

    x = clboot_gfx_console.body_x + (col * clboot_gfx_console.cell_w);
    y = clboot_gfx_console.body_y + (row * clboot_gfx_console.cell_h);
    ch = clboot_gfx_console.cells[row][col];
    clboot_gfx_fill_rect(x, y, clboot_gfx_console.cell_w, clboot_gfx_console.cell_h,
                         clboot_gfx_console.cell_bg[row][col]);
    if (ch != ' ') {
        clboot_gfx_draw_char(x, y + 2U, ch, clboot_gfx_console.cell_fg[row][col], 1U);
    }
}

static void clboot_gfx_console_draw_cursor(void) {
    UINT32 x;
    UINT32 y;

    if (clboot_gfx_console.cursor_visible == 0 || clboot_gfx_console.cursor_row >= clboot_gfx_console.rows ||
        clboot_gfx_console.cursor_col >= clboot_gfx_console.cols) {
        return;
    }

    x = clboot_gfx_console.body_x + (clboot_gfx_console.cursor_col * clboot_gfx_console.cell_w);
    y = clboot_gfx_console.body_y + (clboot_gfx_console.cursor_row * clboot_gfx_console.cell_h);
    clboot_gfx_fill_rect(x, y + clboot_gfx_console.cell_h - 3U, clboot_gfx_console.cell_w, 2U,
                         clboot_gfx_console.accent);
}

static void clboot_gfx_console_erase_cursor(void) {
    if (clboot_gfx_console.cursor_visible != 0) {
        clboot_gfx_console_draw_cell(clboot_gfx_console.cursor_row, clboot_gfx_console.cursor_col);
    }
}

static void clboot_gfx_console_clear_cells(void) {
    UINT32 row;
    UINT32 col;

    for (row = 0U; row < CLBOOT_GFX_CONSOLE_MAX_ROWS; row++) {
        for (col = 0U; col < CLBOOT_GFX_CONSOLE_MAX_COLS; col++) {
            clboot_gfx_console.cells[row][col] = ' ';
            clboot_gfx_console.cell_fg[row][col] = clboot_gfx_console.fg;
            clboot_gfx_console.cell_bg[row][col] = clboot_gfx_console.body_bg;
        }
    }
}

static void clboot_gfx_console_redraw_body(void) {
    UINT32 row;
    UINT32 col;

    clboot_gfx_fill_rect(clboot_gfx_console.body_x, clboot_gfx_console.body_y, clboot_gfx_console.body_w,
                         clboot_gfx_console.body_h, clboot_gfx_console.body_bg);
    for (row = 0U; row < clboot_gfx_console.rows; row++) {
        for (col = 0U; col < clboot_gfx_console.cols; col++) {
            clboot_gfx_console_draw_cell(row, col);
        }
    }
    clboot_gfx_console_draw_cursor();
}

static void clboot_gfx_console_scroll(void) {
    UINT32 row;
    UINT32 col;

    if (clboot_gfx_console.rows <= 1U) {
        clboot_gfx_console_clear_cells();
        clboot_gfx_console.cursor_row = 0U;
        clboot_gfx_console.cursor_col = 0U;
        clboot_gfx_console_redraw_body();
        return;
    }

    for (row = 1U; row < clboot_gfx_console.rows; row++) {
        for (col = 0U; col < clboot_gfx_console.cols; col++) {
            clboot_gfx_console.cells[row - 1U][col] = clboot_gfx_console.cells[row][col];
            clboot_gfx_console.cell_fg[row - 1U][col] = clboot_gfx_console.cell_fg[row][col];
            clboot_gfx_console.cell_bg[row - 1U][col] = clboot_gfx_console.cell_bg[row][col];
        }
    }

    row = clboot_gfx_console.rows - 1U;
    for (col = 0U; col < clboot_gfx_console.cols; col++) {
        clboot_gfx_console.cells[row][col] = ' ';
        clboot_gfx_console.cell_fg[row][col] = clboot_gfx_console.fg;
        clboot_gfx_console.cell_bg[row][col] = clboot_gfx_console.body_bg;
    }

    clboot_gfx_console.cursor_row = row;
    clboot_gfx_console.cursor_col = 0U;
    clboot_gfx_console_redraw_body();
}

static void clboot_gfx_console_newline(void) {
    clboot_gfx_console.cursor_col = 0U;
    clboot_gfx_console.cursor_row++;
    if (clboot_gfx_console.cursor_row >= clboot_gfx_console.rows) {
        clboot_gfx_console_scroll();
    }
}

static void clboot_gfx_console_put_char(char ch) {
    if (clboot_gfx_console.rows == 0U || clboot_gfx_console.cols == 0U) {
        return;
    }

    clboot_gfx_console_erase_cursor();

    if (ch == '\n') {
        clboot_gfx_console_newline();
        clboot_gfx_console_draw_cursor();
        return;
    }
    if (ch == '\r') {
        clboot_gfx_console.cursor_col = 0U;
        clboot_gfx_console_draw_cursor();
        return;
    }
    if (ch == '\b') {
        if (clboot_gfx_console.cursor_col > 0U) {
            clboot_gfx_console.cursor_col--;
        } else if (clboot_gfx_console.cursor_row > 0U) {
            clboot_gfx_console.cursor_row--;
            clboot_gfx_console.cursor_col = clboot_gfx_console.cols - 1U;
        }
        clboot_gfx_console.cells[clboot_gfx_console.cursor_row][clboot_gfx_console.cursor_col] = ' ';
        clboot_gfx_console.cell_fg[clboot_gfx_console.cursor_row][clboot_gfx_console.cursor_col] =
            clboot_gfx_console.fg;
        clboot_gfx_console.cell_bg[clboot_gfx_console.cursor_row][clboot_gfx_console.cursor_col] =
            clboot_gfx_console.body_bg;
        clboot_gfx_console_draw_cell(clboot_gfx_console.cursor_row, clboot_gfx_console.cursor_col);
        clboot_gfx_console_draw_cursor();
        return;
    }
    if (ch == '\t') {
        UINT32 i;
        for (i = 0U; i < 4U; i++) {
            clboot_gfx_console_put_char(' ');
        }
        return;
    }
    if ((unsigned char)ch < 0x20U || (unsigned char)ch >= 0x7FU) {
        ch = '.';
    }

    if (clboot_gfx_console.cursor_col >= clboot_gfx_console.cols) {
        clboot_gfx_console_newline();
    }

    clboot_gfx_console.cells[clboot_gfx_console.cursor_row][clboot_gfx_console.cursor_col] = ch;
    clboot_gfx_console.cell_fg[clboot_gfx_console.cursor_row][clboot_gfx_console.cursor_col] =
        clboot_gfx_console.fg;
    clboot_gfx_console.cell_bg[clboot_gfx_console.cursor_row][clboot_gfx_console.cursor_col] =
        clboot_gfx_console.bg;
    clboot_gfx_console_draw_cell(clboot_gfx_console.cursor_row, clboot_gfx_console.cursor_col);
    clboot_gfx_console.cursor_col++;
    if (clboot_gfx_console.cursor_col >= clboot_gfx_console.cols) {
        clboot_gfx_console_newline();
    }
    clboot_gfx_console_draw_cursor();
}

static void clboot_gfx_console_begin(CHAR16 *subtitle) {
    UINT32 margin = clboot_gfx.width / 14U;
    UINT32 header_y = 48U;

    clboot_memset(&clboot_gfx_console, 0, sizeof(clboot_gfx_console));
    clboot_gfx_console.active = 1;
    clboot_gfx_console.panel_x = margin;
    clboot_gfx_console.panel_y = 38U;
    clboot_gfx_console.panel_w = clboot_gfx.width - (margin * 2U);
    clboot_gfx_console.panel_h = (clboot_gfx.height > 94U) ? (clboot_gfx.height - 76U) : clboot_gfx.height;
    clboot_gfx_console.body_x = clboot_gfx_console.panel_x + 22U;
    clboot_gfx_console.body_y = clboot_gfx_console.panel_y + 88U;
    clboot_gfx_console.body_w = (clboot_gfx_console.panel_w > 44U) ? (clboot_gfx_console.panel_w - 44U) : 1U;
    clboot_gfx_console.body_h = (clboot_gfx_console.panel_h > 118U) ? (clboot_gfx_console.panel_h - 118U) : 1U;
    clboot_gfx_console.cell_w = clboot_gfx.font.width + 1U;
    clboot_gfx_console.cell_h = clboot_gfx.font.height + 4U;
    clboot_gfx_console.cols = clboot_gfx_console.body_w / clboot_gfx_console.cell_w;
    clboot_gfx_console.rows = clboot_gfx_console.body_h / clboot_gfx_console.cell_h;
    if (clboot_gfx_console.cols == 0U) {
        clboot_gfx_console.cols = 1U;
    }
    if (clboot_gfx_console.rows == 0U) {
        clboot_gfx_console.rows = 1U;
    }
    if (clboot_gfx_console.cols > CLBOOT_GFX_CONSOLE_MAX_COLS) {
        clboot_gfx_console.cols = CLBOOT_GFX_CONSOLE_MAX_COLS;
    }
    if (clboot_gfx_console.rows > CLBOOT_GFX_CONSOLE_MAX_ROWS) {
        clboot_gfx_console.rows = CLBOOT_GFX_CONSOLE_MAX_ROWS;
    }
    clboot_gfx_console.fg = clboot_gfx_rgb(231U, 238U, 245U);
    clboot_gfx_console.bg = clboot_gfx_rgb(13U, 20U, 30U);
    clboot_gfx_console.body_bg = clboot_gfx_rgb(13U, 20U, 30U);
    clboot_gfx_console.accent = clboot_gfx_rgb(74U, 186U, 211U);

    clboot_gfx_clear_rgb(18U, 31U, 48U, 4U, 8U, 18U);
    clboot_gfx_fill_rect(clboot_gfx_console.panel_x, clboot_gfx_console.panel_y, clboot_gfx_console.panel_w,
                         clboot_gfx_console.panel_h, clboot_gfx_rgb(22U, 29U, 38U));
    clboot_gfx_rect(clboot_gfx_console.panel_x, clboot_gfx_console.panel_y, clboot_gfx_console.panel_w,
                    clboot_gfx_console.panel_h, clboot_gfx_console.accent);
    clboot_gfx_fill_rect(clboot_gfx_console.panel_x, clboot_gfx_console.panel_y, 7U, clboot_gfx_console.panel_h,
                         clboot_gfx_console.accent);
    clboot_gfx_draw_text(clboot_gfx_console.panel_x + 28U, header_y, "CLBoot", clboot_gfx_console.fg, 2U);
    clboot_gfx_draw_text_wide(clboot_gfx_console.panel_x + 30U, header_y + 50U, subtitle,
                              clboot_gfx_rgb(150U, 164U, 180U), 1U);
    clboot_gfx_fill_rect(clboot_gfx_console.body_x, clboot_gfx_console.body_y, clboot_gfx_console.body_w,
                         clboot_gfx_console.body_h, clboot_gfx_console.body_bg);
    clboot_gfx_rect(clboot_gfx_console.body_x - 1U, clboot_gfx_console.body_y - 1U,
                    clboot_gfx_console.body_w + 2U, clboot_gfx_console.body_h + 2U,
                    clboot_gfx_rgb(38U, 52U, 68U));
    clboot_gfx_console_clear_cells();
    clboot_gfx_console_redraw_body();
}

static void clboot_gfx_console_clear(void) {
    if (clboot_gfx_console_active() == 0) {
        return;
    }
    clboot_gfx_console.cursor_col = 0U;
    clboot_gfx_console.cursor_row = 0U;
    clboot_gfx_console_clear_cells();
    clboot_gfx_console_redraw_body();
}

static void clboot_gfx_console_enable_cursor(int visible) {
    if (clboot_gfx_console_active() == 0) {
        return;
    }
    clboot_gfx_console_erase_cursor();
    clboot_gfx_console.cursor_visible = visible ? 1 : 0;
    clboot_gfx_console_draw_cursor();
}

static void clboot_gfx_console_print(const CHAR16 *text) {
    UINTN i = 0U;

    if (text == (const CHAR16 *)0 || clboot_gfx_console_active() == 0) {
        return;
    }

    while (text[i] != 0U) {
        CHAR16 ch = text[i++];
        clboot_gfx_console_put_char((ch < 128U) ? (char)ch : '?');
    }
}

static void clboot_gfx_console_set_attr(UINTN attr) {
    if (clboot_gfx_console_active() == 0) {
        return;
    }

    clboot_gfx_console.bg = clboot_gfx_console.body_bg;
    if (attr == CLBOOT_TEXT_ATTR_TITLE) {
        clboot_gfx_console.fg = clboot_gfx_rgb(255U, 255U, 255U);
    } else if (attr == CLBOOT_TEXT_ATTR_ACCENT) {
        clboot_gfx_console.fg = clboot_gfx_console.accent;
    } else if (attr == CLBOOT_TEXT_ATTR_SELECTED) {
        clboot_gfx_console.fg = clboot_gfx_rgb(5U, 12U, 20U);
        clboot_gfx_console.bg = clboot_gfx_console.accent;
    } else if (attr == CLBOOT_TEXT_ATTR_WARN) {
        clboot_gfx_console.fg = clboot_gfx_rgb(245U, 190U, 90U);
    } else if (attr == CLBOOT_TEXT_ATTR_ERROR) {
        clboot_gfx_console.fg = clboot_gfx_rgb(240U, 100U, 100U);
    } else if (attr == CLBOOT_TEXT_ATTR_OK) {
        clboot_gfx_console.fg = clboot_gfx_rgb(102U, 214U, 137U);
    } else if (attr == CLBOOT_TEXT_ATTR_DIM) {
        clboot_gfx_console.fg = clboot_gfx_rgb(150U, 164U, 180U);
    } else {
        clboot_gfx_console.fg = clboot_gfx_rgb(231U, 238U, 245U);
    }
}

static void clboot_gfx_draw_menu(const struct clboot_menu_entry *entries, UINTN count, UINTN selected,
                                 UINTN seconds_left, int countdown_active) {
    UINT32 panel = clboot_gfx_rgb(22U, 29U, 38U);
    UINT32 panel2 = clboot_gfx_rgb(31U, 42U, 56U);
    UINT32 accent = clboot_gfx_rgb(74U, 186U, 211U);
    UINT32 text = clboot_gfx_rgb(231U, 238U, 245U);
    UINT32 dim = clboot_gfx_rgb(150U, 164U, 180U);
    UINT32 good = clboot_gfx_rgb(102U, 214U, 137U);
    UINT32 bad = clboot_gfx_rgb(240U, 100U, 100U);
    UINT32 warn = clboot_gfx_rgb(245U, 190U, 90U);
    UINT32 margin = clboot_gfx.width / 12U;
    UINT32 menu_x = margin;
    UINT32 menu_y = clboot_gfx.height / 4U;
    UINT32 menu_w = clboot_gfx.width - (margin * 2U);
    UINT32 row_h = clboot_gfx.font.height + 18U;
    UINTN page_size = 7U;
    UINTN page = selected / page_size;
    UINTN start = page * page_size;
    UINTN end = start + page_size;
    UINTN i;
    char small[48];

    clboot_gfx_console.active = 0;
    if (end > count) {
        end = count;
    }

    clboot_gfx_clear_rgb(18U, 31U, 48U, 4U, 8U, 18U);
    clboot_gfx_fill_rect(menu_x, 58U, 6U, 52U, accent);
    clboot_gfx_draw_text(menu_x + 22U, 54U, "CLBoot", text, 2U);
    clboot_gfx_draw_text(menu_x + 24U, 104U, "Graphical UEFI Boot Manager", dim, 1U);

    clboot_gfx_fill_rect(menu_x, menu_y, menu_w, row_h * (page_size + 2U), panel);
    clboot_gfx_rect(menu_x, menu_y, menu_w, row_h * (page_size + 2U), accent);
    clboot_gfx_draw_text(menu_x + 24U, menu_y + 18U, "Select boot entry", text, 1U);

    for (i = start; i < end; i++) {
        UINT32 y = menu_y + row_h + ((UINT32)(i - start) * row_h);
        UINT32 row_color = (i == selected) ? clboot_gfx_rgb(48U, 78U, 96U) : panel2;
        UINT32 state_color = entries[i].valid != 0 ? good : bad;

        clboot_gfx_fill_rect(menu_x + 16U, y + 4U, menu_w - 32U, row_h - 8U, row_color);
        clboot_gfx_fill_rect(menu_x + 16U, y + 4U, 5U, row_h - 8U, i == selected ? accent : row_color);
        small[0] = (char)('1' + (i - start));
        small[1] = '.';
        small[2] = ' ';
        small[3] = '\0';
        clboot_gfx_draw_text(menu_x + 34U, y + 12U, small, dim, 1U);
        clboot_gfx_draw_text_wide(menu_x + 68U, y + 12U, entries[i].title, text, 1U);
        clboot_gfx_draw_text(menu_x + menu_w - 92U, y + 12U, entries[i].valid != 0 ? "OK" : "BAD", state_color, 1U);
    }

    clboot_gfx_fill_rect(menu_x, clboot_gfx.height - 78U, menu_w, 38U, panel);
    clboot_gfx_rect(menu_x, clboot_gfx.height - 78U, menu_w, 38U, panel2);
    clboot_gfx_draw_text(menu_x + 18U, clboot_gfx.height - 66U,
                         "Enter boot   Arrows select   e edit   c cmdline   i info   s shell   +/- page", dim, 1U);
    if (countdown_active != 0) {
        clboot_gfx_draw_text(menu_x + menu_w - 190U, 110U, "Auto boot", warn, 1U);
        small[0] = (char)('0' + (seconds_left % 10U));
        small[1] = 's';
        small[2] = '\0';
        if (seconds_left >= 10U) {
            small[0] = (char)('0' + ((seconds_left / 10U) % 10U));
            small[1] = (char)('0' + (seconds_left % 10U));
            small[2] = 's';
            small[3] = '\0';
        }
        clboot_gfx_draw_text(menu_x + menu_w - 82U, 110U, small, warn, 1U);
    } else {
        clboot_gfx_draw_text(menu_x + menu_w - 190U, 110U, "Countdown stopped", dim, 1U);
    }

    if (entries[selected].valid == 0) {
        clboot_gfx_draw_text_wide(menu_x + 24U, menu_y + (row_h * (page_size + 2U)) + 18U,
                                  entries[selected].validation, bad, 1U);
    } else {
        clboot_gfx_draw_text_wide(menu_x + 24U, menu_y + (row_h * (page_size + 2U)) + 18U, entries[selected].hint,
                                  dim, 1U);
    }
}

static void clboot_gfx_draw_progress(UINTN percent, CHAR16 *label) {
    UINT32 panel = clboot_gfx_rgb(22U, 29U, 38U);
    UINT32 accent = clboot_gfx_rgb(74U, 186U, 211U);
    UINT32 text = clboot_gfx_rgb(231U, 238U, 245U);
    UINT32 dim = clboot_gfx_rgb(150U, 164U, 180U);
    UINT32 x = clboot_gfx.width / 6U;
    UINT32 y = (clboot_gfx.height / 2U) - 56U;
    UINT32 w = clboot_gfx.width - (x * 2U);
    UINT32 fill_w;

    clboot_gfx_console.active = 0;
    if (percent > 100U) {
        percent = 100U;
    }
    fill_w = (w - 48U) * (UINT32)percent / 100U;

    clboot_gfx_clear_rgb(18U, 31U, 48U, 4U, 8U, 18U);
    clboot_gfx_fill_rect(x, y, w, 132U, panel);
    clboot_gfx_rect(x, y, w, 132U, accent);
    clboot_gfx_draw_text(x + 24U, y + 22U, "CLBoot is preparing CLeonOS", text, 1U);
    clboot_gfx_draw_text_wide(x + 24U, y + 52U, label, dim, 1U);
    clboot_gfx_fill_rect(x + 24U, y + 92U, w - 48U, 14U, clboot_gfx_rgb(38U, 52U, 68U));
    clboot_gfx_fill_rect(x + 24U, y + 92U, fill_w, 14U, accent);
}

static void clboot_gfx_draw_halt(CHAR16 *message) {
    UINT32 panel = clboot_gfx_rgb(22U, 29U, 38U);
    UINT32 accent = clboot_gfx_rgb(74U, 186U, 211U);
    UINT32 text = clboot_gfx_rgb(231U, 238U, 245U);
    UINT32 dim = clboot_gfx_rgb(150U, 164U, 180U);
    UINT32 bad = clboot_gfx_rgb(240U, 100U, 100U);
    UINT32 x = clboot_gfx.width / 6U;
    UINT32 y = (clboot_gfx.height / 2U) - 86U;
    UINT32 w = clboot_gfx.width - (x * 2U);

    clboot_gfx_console.active = 0;
    clboot_gfx_clear_rgb(48U, 20U, 27U, 11U, 7U, 13U);
    clboot_gfx_fill_rect(x, y, w, 172U, panel);
    clboot_gfx_rect(x, y, w, 172U, bad);
    clboot_gfx_fill_rect(x, y, 7U, 172U, bad);
    clboot_gfx_draw_text(x + 28U, y + 22U, "CLBoot stopped", text, 2U);
    clboot_gfx_draw_text(x + 30U, y + 76U, "Boot cannot continue because an error occurred.", dim, 1U);
    clboot_gfx_draw_text_wide(x + 30U, y + 106U, message, bad, 1U);
    clboot_gfx_draw_text(x + 30U, y + 136U, "Power off or reset the machine to retry.", dim, 1U);
    clboot_gfx_fill_rect(x, y + 172U + 20U, w, 3U, accent);
}
