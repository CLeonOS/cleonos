static void clboot_log_append_char(char ch) {
    if (clboot_bootlog_used + 1U >= CLBOOT_LOG_BYTES) {
        return;
    }
    clboot_bootlog[clboot_bootlog_used++] = ch;
    clboot_bootlog[clboot_bootlog_used] = '\0';
}

static void clboot_log_append_ascii(const char *text) {
    UINTN i = 0U;
    if (text == (const char *)0) {
        return;
    }
    while (text[i] != '\0') {
        clboot_log_append_char(text[i++]);
    }
}

static void clboot_log_append_utf16_lossy(CHAR16 *text) {
    UINTN i = 0U;
    if (text == (CHAR16 *)0) {
        return;
    }
    while (text[i] != 0U) {
        CHAR16 ch = text[i++];
        if (ch == '\r') {
            continue;
        }
        clboot_log_append_char((ch >= 32U && ch < 127U) ? (char)ch : '?');
    }
}

static void clboot_print(const CHAR16 *text) {
    if (clboot_gfx_console_active() != 0) {
        clboot_gfx_console_print(text);
        return;
    }

    if (clboot_st != (EFI_SYSTEM_TABLE *)0 && clboot_st->ConOut != (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *)0) {
        (void)clboot_st->ConOut->OutputString(clboot_st->ConOut, (CHAR16 *)text);
    }
}

static void clboot_set_attr(UINTN attr) {
    if (clboot_gfx_console_active() != 0) {
        clboot_gfx_console_set_attr(attr);
        return;
    }

    if (clboot_st != (EFI_SYSTEM_TABLE *)0 && clboot_st->ConOut != (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *)0 &&
        clboot_st->ConOut->SetAttribute != (EFI_TEXT_SET_ATTRIBUTE)0) {
        (void)clboot_st->ConOut->SetAttribute(clboot_st->ConOut, attr);
    }
}

static void clboot_clear_screen(void) {
    if (clboot_gfx_console_active() != 0) {
        clboot_gfx_console_clear();
        return;
    }

    if (clboot_st != (EFI_SYSTEM_TABLE *)0 && clboot_st->ConOut != (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *)0 &&
        clboot_st->ConOut->ClearScreen != (EFI_TEXT_CLEAR_SCREEN)0) {
        (void)clboot_st->ConOut->ClearScreen(clboot_st->ConOut);
    }
}

static void clboot_set_cursor(UINTN col, UINTN row) {
    if (clboot_st != (EFI_SYSTEM_TABLE *)0 && clboot_st->ConOut != (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *)0 &&
        clboot_st->ConOut->SetCursorPosition != (EFI_TEXT_SET_CURSOR_POSITION)0) {
        (void)clboot_st->ConOut->SetCursorPosition(clboot_st->ConOut, col, row);
    }
}

static void clboot_enable_cursor(int visible) {
    if (clboot_gfx_console_active() != 0) {
        clboot_gfx_console_enable_cursor(visible);
        return;
    }

    if (clboot_st != (EFI_SYSTEM_TABLE *)0 && clboot_st->ConOut != (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *)0 &&
        clboot_st->ConOut->EnableCursor != (EFI_TEXT_ENABLE_CURSOR)0) {
        (void)clboot_st->ConOut->EnableCursor(clboot_st->ConOut, visible ? 1U : 0U);
    }
}

static void clboot_select_text_mode(void) {
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out;
    UINTN best_mode = 0U;
    UINTN best_cols = 0U;
    UINTN best_rows = 0U;
    INT32 mode;

    if (clboot_st == (EFI_SYSTEM_TABLE *)0 || clboot_st->ConOut == (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *)0 ||
        clboot_st->ConOut->Mode == (SIMPLE_TEXT_OUTPUT_MODE *)0 ||
        clboot_st->ConOut->QueryMode == (EFI_TEXT_QUERY_MODE)0 ||
        clboot_st->ConOut->SetMode == (EFI_TEXT_SET_MODE)0) {
        return;
    }

    out = clboot_st->ConOut;
    for (mode = 0; mode < out->Mode->MaxMode; mode++) {
        UINTN cols = 0U;
        UINTN rows = 0U;
        if (out->QueryMode(out, (UINTN)mode, &cols, &rows) != EFI_SUCCESS) {
            continue;
        }
        if (cols < 80U || rows < 25U) {
            continue;
        }
        if ((cols * rows) > (best_cols * best_rows)) {
            best_mode = (UINTN)mode;
            best_cols = cols;
            best_rows = rows;
        }
    }

    if (best_cols >= 80U && best_rows >= 25U) {
        (void)out->SetMode(out, best_mode);
    }
}

static void clboot_log(CHAR16 *message) {
    clboot_log_append_utf16_lossy(message);
    clboot_log_append_char('\n');
    clboot_bootlog_entries++;
}

static void clboot_status(CHAR16 *message) {
    if (clboot_gfx_available() != 0) {
        clboot_log(message);
        return;
    }

    clboot_print(L"  ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_OK);
    clboot_print(L"* ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(message);
    clboot_print(L"\r\n");
    clboot_log(message);
}

static void clboot_draw_box_top(UINTN width) {
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L"  +");
    clboot_print_repeat(L'=', width);
    clboot_print(L"+\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
}

static void clboot_draw_box_bottom(UINTN width) {
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L"  +");
    clboot_print_repeat(L'=', width);
    clboot_print(L"+\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
}

static void clboot_draw_box_line(CHAR16 *left, CHAR16 *right, UINTN width) {
    UINTN payload_width = (width > 2U) ? (width - 2U) : width;
    UINTN left_width = payload_width;
    UINTN right_len = clboot_wstrlen(right);

    if (right_len + 2U < payload_width) {
        left_width = payload_width - right_len - 2U;
    }

    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L"  | ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print_padded(left, left_width);
    if (right != (CHAR16 *)0 && right_len > 0U && right_len + 2U < payload_width) {
        clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
        clboot_print(L"  ");
        clboot_print(right);
        clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    }
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L" |\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
}

static void clboot_draw_brand_header(CHAR16 *mode) {
    clboot_clear_screen();
    clboot_set_cursor(0U, 0U);
    clboot_print(L"\r\n");
    clboot_draw_box_top(76U);
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L"  | ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_TITLE);
    clboot_print_padded(L"CLeonOS CLBoot", 74U);
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L" |\r\n");
    clboot_draw_box_line(L"independent UEFI loader for CLKS", L"x86_64 ISO", 76U);
    clboot_draw_box_line(L"protocol v2 / bootlog / graphical handoff", L"UEFI text UI", 76U);
    clboot_draw_box_bottom(76U);
    if (mode != (CHAR16 *)0) {
        clboot_print(L"  ");
        clboot_set_attr(CLBOOT_TEXT_ATTR_OK);
        clboot_print(L"> ");
        clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
        clboot_print(mode);
        clboot_print(L"\r\n");
    }
    clboot_print(L"\r\n");
}

static void clboot_draw_footer(CHAR16 *text) {
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L"  ");
    clboot_print(text);
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
}

static void clboot_draw_compact_header(CHAR16 *subtitle) {
    if (clboot_gfx_available() != 0) {
        clboot_gfx_console_begin(subtitle);
        return;
    }

    clboot_clear_screen();
    clboot_set_cursor(0U, 0U);
    clboot_set_attr(CLBOOT_TEXT_ATTR_TITLE);
    clboot_print(L" CLeonOS CLBoot UEFI");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L"  x86_64 ISO / protocol v2");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L" ------------------------------------------------------------------------------\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_OK);
    clboot_print(L" ");
    clboot_print(subtitle);
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(L"\r\n");
}

static void clboot_halt(CHAR16 *message) {
    clboot_log(message);
    if (clboot_gfx_available() != 0) {
        clboot_gfx_draw_halt(message);
        for (;;) {
            __asm__ volatile("hlt");
        }
    }

    clboot_draw_brand_header(L"Boot halted");
    clboot_draw_box_top(76U);
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L"  | ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_ERROR);
    clboot_print_padded(L"ERROR", 74U);
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L" |\r\n");
    clboot_draw_box_line(message, L"", 76U);
    clboot_draw_box_line(L"Check the CLBoot log above or rebuild with verbose boot enabled.", L"", 76U);
    clboot_draw_box_bottom(76U);
    clboot_draw_footer(L"The machine is halted. Power off or reset to retry.");
    for (;;) {
        __asm__ volatile("hlt");
    }
}

static void clboot_draw_progress(UINTN percent, CHAR16 *label) {
    UINTN i;
    UINTN filled = percent / 4U;

    if (clboot_gfx_available() != 0) {
        clboot_gfx_draw_progress(percent, label);
        return;
    }

    if (filled > 25U) {
        filled = 25U;
    }

    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(L"[");
    for (i = 0U; i < 25U; i++) {
        if (i < filled) {
            clboot_set_attr(CLBOOT_TEXT_ATTR_OK);
            clboot_print(L"#");
        } else {
            clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
            clboot_print(L"-");
        }
    }
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(L"] ");
    if (percent < 100U) {
        clboot_print(L" ");
    }
    if (percent < 10U) {
        clboot_print(L" ");
    }
    clboot_print_uint(percent);
    clboot_print(L"%");
    clboot_print(L"  ");
    clboot_print(label);
    clboot_print(L"\r\n");
}

static EFI_STATUS clboot_read_key(EFI_INPUT_KEY *key) {
    if (clboot_st == (EFI_SYSTEM_TABLE *)0 || clboot_st->ConIn == (EFI_SIMPLE_TEXT_INPUT_PROTOCOL *)0 ||
        clboot_st->ConIn->ReadKeyStroke == (EFI_INPUT_READ_KEY)0) {
        return EFI_NOT_FOUND;
    }
    return clboot_st->ConIn->ReadKeyStroke(clboot_st->ConIn, key);
}

static int clboot_poll_key(EFI_INPUT_KEY *key) {
    return (clboot_read_key(key) == EFI_SUCCESS) ? 1 : 0;
}

static void clboot_reset_input(void) {
    EFI_INPUT_KEY key;
    if (clboot_st != (EFI_SYSTEM_TABLE *)0 && clboot_st->ConIn != (EFI_SIMPLE_TEXT_INPUT_PROTOCOL *)0 &&
        clboot_st->ConIn->Reset != (EFI_INPUT_RESET)0) {
        (void)clboot_st->ConIn->Reset(clboot_st->ConIn, 0U);
    }
    while (clboot_poll_key(&key) != 0) {
    }
}
