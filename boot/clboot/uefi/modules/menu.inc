static void clboot_menu_entry_init(struct clboot_menu_entry *entry, const char *title, const char *hint,
                                   const char *kernel_path, const char *ramdisk_path, const char *cmdline,
                                   const char *source) {
    if (entry == (struct clboot_menu_entry *)0) {
        return;
    }
    clboot_ascii_to_wide(entry->title, CLBOOT_MENU_TITLE_MAX, title);
    clboot_ascii_to_wide(entry->hint, CLBOOT_MENU_HINT_MAX, hint);
    clboot_ascii_to_wide(entry->kernel_path, CLBOOT_MENU_PATH_MAX, kernel_path);
    clboot_ascii_to_wide(entry->ramdisk_path, CLBOOT_MENU_PATH_MAX, ramdisk_path);
    clboot_ascii_to_wide(entry->source, CLBOOT_MENU_HINT_MAX, source);
    clboot_ascii_to_wide(entry->validation, CLBOOT_MENU_HINT_MAX, "not checked");
    entry->kernel_ok = 0;
    entry->ramdisk_ok = 0;
    entry->valid = 0;
    clboot_copy_ascii(entry->cmdline, CLBOOT_MENU_CMDLINE_MAX, cmdline);
}

static struct clboot_menu_entry *clboot_menu_add(struct clboot_menu_config *cfg, const char *title, const char *hint,
                                                 const char *kernel_path, const char *ramdisk_path,
                                                 const char *cmdline, const char *source) {
    struct clboot_menu_entry *entry;

    if (cfg == (struct clboot_menu_config *)0 || cfg->count >= CLBOOT_MENU_MAX_ENTRIES) {
        return (struct clboot_menu_entry *)0;
    }

    entry = &cfg->entries[cfg->count++];
    clboot_menu_entry_init(entry, title, hint, kernel_path, ramdisk_path, cmdline, source);
    return entry;
}

static void clboot_menu_config_init_defaults(struct clboot_menu_config *cfg) {
    if (cfg == (struct clboot_menu_config *)0) {
        return;
    }

    clboot_memset(cfg, 0, sizeof(*cfg));
    cfg->timeout_seconds = CLBOOT_MENU_TIMEOUT_SECONDS;
    clboot_copy_ascii(cfg->global_cmdline, sizeof(cfg->global_cmdline), CLBOOT_DEFAULT_CMDLINE);

    (void)clboot_menu_add(cfg, "Try CLeonOS", "Normal ISO live boot.", "\\boot\\clks_kernel.elf",
                          "\\boot\\cleonos_ramdisk.tar", "", "builtin");
    (void)clboot_menu_add(cfg, "Safe Mode", "No splash, root rescue mode, verbose logs.", "\\boot\\clks_kernel.elf",
                          "\\boot\\cleonos_ramdisk.tar", "clks.rescue=1 clks.nosplash clks.loglevel=debug",
                          "builtin");
    (void)clboot_menu_add(cfg, "Verbose Boot", "Normal boot with debug-level kernel logs.", "\\boot\\clks_kernel.elf",
                          "\\boot\\cleonos_ramdisk.tar", "clks.loglevel=debug", "builtin");
    (void)clboot_menu_add(cfg, "Quiet Boot", "Normal boot with quiet kernel logging.", "\\boot\\clks_kernel.elf",
                          "\\boot\\cleonos_ramdisk.tar", "clks.loglevel=quiet", "builtin");
    (void)clboot_menu_add(cfg, "Install to Disk", "Boot and auto-run install2disk install.",
                          "\\boot\\clks_kernel.elf", "\\boot\\cleonos_ramdisk.tar",
                          "clks.installer=install clks.nosplash", "builtin");
    (void)clboot_menu_add(cfg, "Repair Disk System", "Boot and auto-run install2disk repair.",
                          "\\boot\\clks_kernel.elf", "\\boot\\cleonos_ramdisk.tar",
                          "clks.installer=repair clks.nosplash", "builtin");
    (void)clboot_menu_add(cfg, "Update Kernel", "Boot and auto-run install2disk update-kernel.",
                          "\\boot\\clks_kernel.elf", "\\boot\\cleonos_ramdisk.tar",
                          "clks.installer=update-kernel clks.nosplash", "builtin");
    (void)clboot_menu_add(cfg, "Verify Installation", "Boot and auto-run install2disk verify.",
                          "\\boot\\clks_kernel.elf", "\\boot\\cleonos_ramdisk.tar",
                          "clks.installer=verify clks.nosplash", "builtin");
}

static UINTN clboot_parse_uint(const char *text, UINTN len, UINTN fallback) {
    UINTN value = 0U;
    UINTN i;
    int any = 0;

    if (text == (const char *)0) {
        return fallback;
    }
    for (i = 0U; i < len && text[i] != '\0'; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return fallback;
        }
        value = (value * 10U) + (UINTN)(text[i] - '0');
        any = 1;
    }
    return any != 0 ? value : fallback;
}

static void clboot_trim_line(const char *data, UINTN len, UINTN *out_start, UINTN *out_len) {
    UINTN start = 0U;
    UINTN end = len;

    while (start < len && (data[start] == ' ' || data[start] == '\t')) {
        start++;
    }
    while (end > start && (data[end - 1U] == ' ' || data[end - 1U] == '\t' || data[end - 1U] == '\r')) {
        end--;
    }
    *out_start = start;
    *out_len = end - start;
}

static int clboot_line_key_value(const char *line, UINTN line_len, const char **out_key, UINTN *out_key_len,
                                 const char **out_value, UINTN *out_value_len) {
    UINTN eq = 0U;
    UINTN key_start;
    UINTN key_len;
    UINTN value_start;
    UINTN value_len;

    while (eq < line_len && line[eq] != '=') {
        eq++;
    }
    if (eq == line_len) {
        return 0;
    }

    clboot_trim_line(line, eq, &key_start, &key_len);
    clboot_trim_line(line + eq + 1U, line_len - eq - 1U, &value_start, &value_len);
    *out_key = line + key_start;
    *out_key_len = key_len;
    *out_value = line + eq + 1U + value_start;
    *out_value_len = value_len;
    return 1;
}

static void clboot_menu_parse_config(struct clboot_menu_config *cfg) {
    struct clboot_loaded_file config;
    char *data;
    uint64_t pos;
    struct clboot_menu_entry *current = (struct clboot_menu_entry *)0;
    UINTN old_count;
    CHAR16 *config_paths[] = {
        CLBOOT_CONFIG_PATH,
        L"\\EFI\\CLEONOS\\CLBOOT.CONF",
        L"\\EFI\\CLEONOS\\CLBOOT~1.CON",
        (CHAR16 *)0,
    };

    if (cfg == (struct clboot_menu_config *)0) {
        return;
    }

    old_count = cfg->count;
    if (clboot_read_file_any(config_paths, &config) == 0 || config.size == 0ULL) {
        return;
    }

    data = (char *)config.data;
    for (pos = 0ULL; pos < config.size;) {
        uint64_t line_start = pos;
        UINTN raw_len;
        UINTN trim_start;
        UINTN trim_len;
        const char *line;
        const char *key;
        const char *value;
        UINTN key_len;
        UINTN value_len;

        while (pos < config.size && data[pos] != '\n') {
            pos++;
        }
        raw_len = (UINTN)(pos - line_start);
        if (pos < config.size && data[pos] == '\n') {
            pos++;
        }

        clboot_trim_line(data + line_start, raw_len, &trim_start, &trim_len);
        if (trim_len == 0U) {
            continue;
        }
        line = data + line_start + trim_start;
        if (line[0] == '#') {
            continue;
        }
        if (clboot_line_key_value(line, trim_len, &key, &key_len, &value, &value_len) == 0) {
            continue;
        }

        if (clboot_str_equal_n(key, "timeout", key_len) != 0) {
            cfg->timeout_seconds = clboot_parse_uint(value, value_len, cfg->timeout_seconds);
        } else if (clboot_str_equal_n(key, "default", key_len) != 0) {
            cfg->default_index = clboot_parse_uint(value, value_len, cfg->default_index);
        } else if (clboot_str_equal_n(key, "cmdline", key_len) != 0 && current == (struct clboot_menu_entry *)0) {
            clboot_copy_ascii_n(cfg->global_cmdline, sizeof(cfg->global_cmdline), value, value_len);
        } else if (clboot_str_equal_n(key, "menuentry", key_len) != 0) {
            if (cfg->count == old_count) {
                cfg->count = 0U;
            }
            current = clboot_menu_add(cfg, "", "Configured boot entry.", "\\boot\\clks_kernel.elf",
                                      "\\boot\\cleonos_ramdisk.tar", "", "clboot.conf");
            if (current != (struct clboot_menu_entry *)0) {
                clboot_ascii_n_to_wide(current->title, CLBOOT_MENU_TITLE_MAX, value, value_len);
            }
        } else if (current != (struct clboot_menu_entry *)0) {
            if (clboot_str_equal_n(key, "kernel", key_len) != 0) {
                clboot_ascii_n_to_wide(current->kernel_path, CLBOOT_MENU_PATH_MAX, value, value_len);
            } else if (clboot_str_equal_n(key, "ramdisk", key_len) != 0) {
                clboot_ascii_n_to_wide(current->ramdisk_path, CLBOOT_MENU_PATH_MAX, value, value_len);
            } else if (clboot_str_equal_n(key, "cmdline", key_len) != 0) {
                clboot_copy_ascii_n(current->cmdline, CLBOOT_MENU_CMDLINE_MAX, value, value_len);
            } else if (clboot_str_equal_n(key, "hint", key_len) != 0) {
                clboot_ascii_n_to_wide(current->hint, CLBOOT_MENU_HINT_MAX, value, value_len);
            }
        }
    }

    if (cfg->count == 0U) {
        clboot_menu_config_init_defaults(cfg);
    }
    if (cfg->default_index >= cfg->count) {
        cfg->default_index = 0U;
    }
}

static int clboot_wide_ends_with_elf(CHAR16 *name) {
    UINTN len = clboot_wstrlen(name);
    CHAR16 a;
    CHAR16 b;
    CHAR16 c;
    CHAR16 d;

    if (len < 4U) {
        return 0;
    }
    a = name[len - 4U];
    b = name[len - 3U];
    c = name[len - 2U];
    d = name[len - 1U];
    if (b >= L'A' && b <= L'Z') {
        b = (CHAR16)(b - L'A' + L'a');
    }
    if (c >= L'A' && c <= L'Z') {
        c = (CHAR16)(c - L'A' + L'a');
    }
    if (d >= L'A' && d <= L'Z') {
        d = (CHAR16)(d - L'A' + L'a');
    }
    return (a == L'.' && b == L'e' && c == L'l' && d == L'f') ? 1 : 0;
}

static void clboot_wide_path_join(CHAR16 *dst, UINTN dst_size, CHAR16 *dir, CHAR16 *name) {
    UINTN pos = 0U;
    UINTN i = 0U;

    if (dst == (CHAR16 *)0 || dst_size == 0U) {
        return;
    }
    while (pos + 1U < dst_size && dir != (CHAR16 *)0 && dir[i] != 0U) {
        dst[pos++] = dir[i++];
    }
    if (pos > 0U && dst[pos - 1U] != L'\\' && pos + 1U < dst_size) {
        dst[pos++] = L'\\';
    }
    i = 0U;
    while (pos + 1U < dst_size && name != (CHAR16 *)0 && name[i] != 0U) {
        dst[pos++] = name[i++];
    }
    dst[pos] = 0U;
}

static void clboot_menu_scan_kernel_dir(struct clboot_menu_config *cfg) {
    EFI_FILE_PROTOCOL *root;
    EFI_FILE_PROTOCOL *dir;
    UINT8 info_buf[512];
    static CHAR16 kernel_dir[] = L"\\boot\\kernels";

    if (cfg == (struct clboot_menu_config *)0 || cfg->count >= CLBOOT_MENU_MAX_ENTRIES) {
        return;
    }

    root = clboot_open_root();
    if (root == (EFI_FILE_PROTOCOL *)0) {
        return;
    }
    if (root->Open(root, &dir, kernel_dir, EFI_FILE_MODE_READ, 0ULL) != EFI_SUCCESS) {
        (void)root->Close(root);
        return;
    }

    for (;;) {
        EFI_FILE_INFO *info = (EFI_FILE_INFO *)info_buf;
        UINTN got = sizeof(info_buf);
        struct clboot_menu_entry *entry;
        CHAR16 kernel_path[CLBOOT_MENU_PATH_MAX];

        if (dir->Read(dir, &got, info_buf) != EFI_SUCCESS || got == 0U) {
            break;
        }
        if ((info->Attribute & EFI_FILE_DIRECTORY) != 0ULL) {
            continue;
        }
        if (clboot_wide_ends_with_elf(info->FileName) == 0) {
            continue;
        }
        if (cfg->count >= CLBOOT_MENU_MAX_ENTRIES) {
            break;
        }

        clboot_wide_path_join(kernel_path, CLBOOT_MENU_PATH_MAX, kernel_dir, info->FileName);
        entry = &cfg->entries[cfg->count++];
        clboot_copy_wide(entry->title, CLBOOT_MENU_TITLE_MAX, info->FileName);
        clboot_copy_wide(entry->hint, CLBOOT_MENU_HINT_MAX, L"Kernel from /boot/kernels.");
        clboot_copy_wide(entry->kernel_path, CLBOOT_MENU_PATH_MAX, kernel_path);
        clboot_ascii_to_wide(entry->ramdisk_path, CLBOOT_MENU_PATH_MAX, "\\boot\\cleonos_ramdisk.tar");
        clboot_copy_wide(entry->source, CLBOOT_MENU_HINT_MAX, L"/boot/kernels scan");
        clboot_ascii_to_wide(entry->validation, CLBOOT_MENU_HINT_MAX, "not checked");
        entry->kernel_ok = 0;
        entry->ramdisk_ok = 0;
        entry->valid = 0;
        clboot_copy_ascii(entry->cmdline, CLBOOT_MENU_CMDLINE_MAX, "");
    }

    (void)dir->Close(dir);
    (void)root->Close(root);
}

static void clboot_menu_validate(struct clboot_menu_config *cfg) {
    UINTN i;

    if (cfg == (struct clboot_menu_config *)0) {
        return;
    }
    for (i = 0U; i < cfg->count; i++) {
        struct clboot_menu_entry *entry = &cfg->entries[i];
        entry->kernel_ok = clboot_file_exists(entry->kernel_path);
        entry->ramdisk_ok = (clboot_path_is_empty_or_none(entry->ramdisk_path) != 0)
                                 ? 1
                                 : clboot_file_exists(entry->ramdisk_path);
        entry->valid = (entry->kernel_ok != 0 && entry->ramdisk_ok != 0) ? 1 : 0;

        if (entry->valid != 0) {
            clboot_ascii_to_wide(entry->validation, CLBOOT_MENU_HINT_MAX, "OK");
        } else if (entry->kernel_ok == 0 && entry->ramdisk_ok == 0) {
            clboot_ascii_to_wide(entry->validation, CLBOOT_MENU_HINT_MAX, "missing kernel and ramdisk");
        } else if (entry->kernel_ok == 0) {
            clboot_ascii_to_wide(entry->validation, CLBOOT_MENU_HINT_MAX, "missing kernel");
        } else {
            clboot_ascii_to_wide(entry->validation, CLBOOT_MENU_HINT_MAX, "missing ramdisk");
        }
    }
}

static char *clboot_cmdline_build(const char *base, const char *extra) {
    char *cmdline;
    UINTN base_len = clboot_strlen(base);
    UINTN extra_len = clboot_strlen(extra);
    UINTN need_space = (base_len > 0U && extra_len > 0U) ? 1U : 0U;

    if (clboot_alloc_pool(base_len + need_space + extra_len + 1U, (void **)&cmdline) == 0) {
        return (char *)0;
    }
    if (base_len > 0U) {
        clboot_memcpy(cmdline, base, base_len);
    }
    if (need_space != 0U) {
        cmdline[base_len] = ' ';
    }
    if (extra_len > 0U) {
        clboot_memcpy(cmdline + base_len + need_space, extra, extra_len);
    }
    cmdline[base_len + need_space + extra_len] = '\0';
    return cmdline;
}

static void clboot_draw_cmdline_editor(struct clboot_menu_entry *entry, UINTN cursor) {
    clboot_draw_compact_header(L"Edit boot arguments");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" Entry: ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(entry->title);
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" Kernel: ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(entry->kernel_path);
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L" --------------------------------------------------------------------------\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(L" ");
    clboot_print(L"cmdline: ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_WARN);
    clboot_print((CHAR16 *)L"(entry arguments only)");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(L"\r\n ");
    {
        UINTN i = 0U;
        while (entry->cmdline[i] != '\0') {
            CHAR16 ch[2];
            if (i == cursor) {
                clboot_set_attr(CLBOOT_TEXT_ATTR_SELECTED);
            }
            ch[0] = (CHAR16)(unsigned char)entry->cmdline[i];
            ch[1] = 0U;
            clboot_print(ch);
            if (i == cursor) {
                clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
            }
            i++;
        }
        if (cursor == i) {
            clboot_set_attr(CLBOOT_TEXT_ATTR_SELECTED);
            clboot_print(L" ");
            clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
        }
    }
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L" --------------------------------------------------------------------------\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" Type to edit   Backspace: delete   Enter: save   Esc: cancel\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
}

static void clboot_edit_cmdline(struct clboot_menu_entry *entry) {
    char original[CLBOOT_MENU_CMDLINE_MAX];
    UINTN cursor;
    EFI_INPUT_KEY key;

    if (entry == (struct clboot_menu_entry *)0) {
        return;
    }
    clboot_copy_ascii(original, sizeof(original), entry->cmdline);
    cursor = clboot_strlen(entry->cmdline);
    clboot_draw_cmdline_editor(entry, cursor);

    for (;;) {
        if (clboot_poll_key(&key) == 0) {
            if (clboot_bs != (EFI_BOOT_SERVICES *)0 && clboot_bs->Stall != (void *)0) {
                (void)clboot_bs->Stall(20000U);
            }
            continue;
        }
        if (key.UnicodeChar == L'\r' || key.UnicodeChar == L'\n') {
            return;
        }
        if (key.ScanCode == 23U) {
            clboot_copy_ascii(entry->cmdline, sizeof(entry->cmdline), original);
            return;
        }
        if (key.UnicodeChar == 8U) {
            if (cursor > 0U) {
                UINTN len = clboot_strlen(entry->cmdline);
                UINTN i;
                for (i = cursor - 1U; i < len; i++) {
                    entry->cmdline[i] = entry->cmdline[i + 1U];
                }
                cursor--;
            }
        } else if (key.UnicodeChar >= 32U && key.UnicodeChar < 127U) {
            UINTN len = clboot_strlen(entry->cmdline);
            if (len + 1U < sizeof(entry->cmdline)) {
                UINTN i;
                for (i = len + 1U; i > cursor; i--) {
                    entry->cmdline[i] = entry->cmdline[i - 1U];
                }
                entry->cmdline[cursor] = (char)key.UnicodeChar;
                cursor++;
            }
        } else if (key.ScanCode == 3U) {
            if (cursor > 0U) {
                cursor--;
            }
        } else if (key.ScanCode == 4U) {
            if (cursor < clboot_strlen(entry->cmdline)) {
                cursor++;
            }
        }
        clboot_draw_cmdline_editor(entry, cursor);
    }
}

static void clboot_wait_any_key(void) {
    EFI_INPUT_KEY key;

    clboot_reset_input();
    for (;;) {
        if (clboot_poll_key(&key) != 0) {
            return;
        }
        if (clboot_bs != (EFI_BOOT_SERVICES *)0 && clboot_bs->Stall != (void *)0) {
            (void)clboot_bs->Stall(20000U);
        }
    }
}

static void clboot_print_ascii_wrapped(const char *text, UINTN width) {
    UINTN col = 0U;
    UINTN i = 0U;

    if (text == (const char *)0) {
        return;
    }
    while (text[i] != '\0') {
        CHAR16 ch[2];
        if (col >= width && text[i] == ' ') {
            clboot_print(L"\r\n ");
            col = 0U;
            i++;
            continue;
        }
        if (col >= width) {
            clboot_print(L"\r\n ");
            col = 0U;
        }
        ch[0] = (CHAR16)(unsigned char)text[i++];
        ch[1] = 0U;
        clboot_print(ch);
        col++;
    }
}

static void clboot_show_cmdline_preview(const struct clboot_menu_config *cfg, const struct clboot_menu_entry *entry) {
    char *final_cmdline;

    final_cmdline = clboot_cmdline_build(cfg->global_cmdline, entry->cmdline);
    clboot_draw_compact_header(L"Final command line preview");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" Entry: ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(entry->title);
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" Formula: global cmdline + entry cmdline\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L" --------------------------------------------------------------------------\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(L" ");
    clboot_print_ascii_wrapped(final_cmdline == (char *)0 ? "<allocation failed>" : final_cmdline, 76U);
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L" --------------------------------------------------------------------------\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" Press any key to return to Boot Menu.\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_wait_any_key();
}

static void clboot_show_entry_info(const struct clboot_menu_entry *entry) {
    clboot_draw_compact_header(L"Boot entry details");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" Title   : ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(entry->title);
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" Kernel  : ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(entry->kernel_path);
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" Ramdisk : ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(entry->ramdisk_path);
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" Source  : ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(entry->source);
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" Check   : ");
    clboot_set_attr(entry->valid != 0 ? CLBOOT_TEXT_ATTR_OK : CLBOOT_TEXT_ATTR_ERROR);
    clboot_print(entry->validation);
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" Hint    : ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(entry->hint);
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" Cmdline : ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print_ascii_wrapped(entry->cmdline[0] == '\0' ? "<empty>" : entry->cmdline, 66U);
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L" --------------------------------------------------------------------------\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" Press any key to return to Boot Menu.\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_wait_any_key();
}

static void clboot_draw_menu(const struct clboot_menu_entry *entries, UINTN count, UINTN selected,
                             UINTN seconds_left, int countdown_active) {
    UINTN i;
    UINTN page_size = 8U;
    UINTN page = selected / page_size;
    UINTN start = page * page_size;
    UINTN end = start + page_size;

    if (clboot_gfx_available() != 0) {
        clboot_gfx_draw_menu(entries, count, selected, seconds_left, countdown_active);
        return;
    }

    if (end > count) {
        end = count;
    }

    clboot_draw_compact_header(L"Boot menu");
    clboot_print(L" ");
    clboot_set_attr(CLBOOT_TEXT_ATTR_WARN);
    if (countdown_active != 0) {
        clboot_print(L"Auto boot: ");
        clboot_print_uint(seconds_left);
        clboot_print(L"s   ");
    } else {
        clboot_print(L"Auto boot: stopped   ");
    }
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(L"Enter: boot   e: edit   c: cmdline   i: info   s: shell   +/-: page\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L" --------------------------------------------------------------------------\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);

    for (i = start; i < end; i++) {
        CHAR16 prefix[8];
        prefix[0] = L' ';
        prefix[1] = (i == selected) ? L'>' : L' ';
        prefix[2] = L' ';
        prefix[3] = (CHAR16)(L'1' + (i - start));
        prefix[4] = L'.';
        prefix[5] = L' ';
        prefix[6] = 0U;

        clboot_set_attr((i == selected) ? CLBOOT_TEXT_ATTR_SELECTED : CLBOOT_TEXT_ATTR_NORMAL);
        clboot_print(prefix);
        clboot_print_padded(entries[i].title, 62U);
        if (entries[i].valid == 0) {
            clboot_set_attr(CLBOOT_TEXT_ATTR_ERROR);
            clboot_print(L" [BAD]");
            clboot_set_attr((i == selected) ? CLBOOT_TEXT_ATTR_SELECTED : CLBOOT_TEXT_ATTR_NORMAL);
            clboot_print_padded(L"", 5U);
        } else {
            clboot_set_attr(CLBOOT_TEXT_ATTR_OK);
            clboot_print(L" [OK] ");
        }
        clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
        clboot_print(L"\r\n");
    }

    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L" --------------------------------------------------------------------------\r\n");
    clboot_set_attr(entries[selected].valid == 0 ? CLBOOT_TEXT_ATTR_ERROR : CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L" ");
    if (entries[selected].valid == 0) {
        clboot_print(L"Invalid entry: ");
        clboot_print(entries[selected].validation);
        clboot_print(L". Press i for details.");
    } else {
        clboot_print(entries[selected].hint);
    }
    clboot_print(L"\r\n Page ");
    clboot_print_uint(page + 1U);
    clboot_print(L"/");
    clboot_print_uint((count + page_size - 1U) / page_size);
    clboot_print(L"   Bootlog will be passed to CLKS through protocol v2.\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
}

static UINTN clboot_boot_menu_select(struct clboot_menu_config *cfg) {
    UINTN count = cfg->count;
    UINTN selected = cfg->default_index;
    UINTN seconds_left = cfg->timeout_seconds;
    int countdown_active = (seconds_left > 0U && cfg->entries[selected].valid != 0) ? 1 : 0;
    UINTN ticks;
    EFI_INPUT_KEY key;

    clboot_reset_input();
    clboot_enable_cursor(0);
    clboot_draw_menu(cfg->entries, count, selected, seconds_left, countdown_active);

    for (;;) {
        for (ticks = 0U; ticks < 10U; ticks++) {
            if (clboot_poll_key(&key) != 0) {
                if (key.UnicodeChar == L'\r' || key.UnicodeChar == L'\n') {
                    if (cfg->entries[selected].valid == 0) {
                        countdown_active = 0;
                        clboot_show_entry_info(&cfg->entries[selected]);
                        clboot_draw_menu(cfg->entries, count, selected, seconds_left, countdown_active);
                        continue;
                    }
                    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
                    return selected;
                }
                if ((key.UnicodeChar == L'e' || key.UnicodeChar == L'E') && selected < count) {
                    countdown_active = 0;
                    clboot_edit_cmdline(&cfg->entries[selected]);
                    clboot_draw_menu(cfg->entries, count, selected, seconds_left, countdown_active);
                    continue;
                }
                if ((key.UnicodeChar == L'c' || key.UnicodeChar == L'C') && selected < count) {
                    countdown_active = 0;
                    clboot_show_cmdline_preview(cfg, &cfg->entries[selected]);
                    clboot_draw_menu(cfg->entries, count, selected, seconds_left, countdown_active);
                    continue;
                }
                if ((key.UnicodeChar == L'i' || key.UnicodeChar == L'I') && selected < count) {
                    countdown_active = 0;
                    clboot_show_entry_info(&cfg->entries[selected]);
                    clboot_draw_menu(cfg->entries, count, selected, seconds_left, countdown_active);
                    continue;
                }
                if ((key.UnicodeChar == L's' || key.UnicodeChar == L'S') && selected < count) {
                    countdown_active = 0;
                    clboot_shell_run(cfg, selected);
                    clboot_draw_menu(cfg->entries, count, selected, seconds_left, countdown_active);
                    continue;
                }
                if (key.UnicodeChar >= L'1' && key.UnicodeChar <= L'8') {
                    UINTN page_start = (selected / 8U) * 8U;
                    UINTN next = page_start + (UINTN)(key.UnicodeChar - L'1');
                    if (next >= count) {
                        continue;
                    }
                    countdown_active = 0;
                    selected = next;
                    clboot_draw_menu(cfg->entries, count, selected, seconds_left, countdown_active);
                    continue;
                }
                if (key.UnicodeChar == L'+' || key.UnicodeChar == L'=') {
                    UINTN next = ((selected / 8U) + 1U) * 8U;
                    countdown_active = 0;
                    if (next < count) {
                        selected = next;
                    }
                    clboot_draw_menu(cfg->entries, count, selected, seconds_left, countdown_active);
                    continue;
                }
                if (key.UnicodeChar == L'-' || key.UnicodeChar == L'_') {
                    UINTN page = selected / 8U;
                    countdown_active = 0;
                    if (page > 0U) {
                        selected = (page - 1U) * 8U;
                    }
                    clboot_draw_menu(cfg->entries, count, selected, seconds_left, countdown_active);
                    continue;
                }
                if (key.ScanCode == 1U) {
                    countdown_active = 0;
                    selected = (selected == 0U) ? (count - 1U) : (selected - 1U);
                    clboot_draw_menu(cfg->entries, count, selected, seconds_left, countdown_active);
                    continue;
                }
                if (key.ScanCode == 2U) {
                    countdown_active = 0;
                    selected = (selected + 1U) % count;
                    clboot_draw_menu(cfg->entries, count, selected, seconds_left, countdown_active);
                    continue;
                }
            }
            if (clboot_bs != (EFI_BOOT_SERVICES *)0 && clboot_bs->Stall != (void *)0) {
                (void)clboot_bs->Stall(100000U);
            }
        }
        if (countdown_active != 0) {
            if (seconds_left == 0U) {
                break;
            }
            seconds_left--;
            clboot_draw_menu(cfg->entries, count, selected, seconds_left, countdown_active);
            if (seconds_left == 0U) {
                break;
            }
        }
    }

    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    return selected;
}
