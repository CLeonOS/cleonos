static int clboot_shell_streq(const char *left, const char *right) {
    UINTN i = 0U;

    if (left == (const char *)0 || right == (const char *)0) {
        return 0;
    }

    while (left[i] != '\0' && right[i] != '\0') {
        if (left[i] != right[i]) {
            return 0;
        }
        i++;
    }

    return left[i] == '\0' && right[i] == '\0';
}

static int clboot_shell_starts_with(const char *text, const char *prefix) {
    UINTN i = 0U;

    if (text == (const char *)0 || prefix == (const char *)0) {
        return 0;
    }

    while (prefix[i] != '\0') {
        if (text[i] != prefix[i]) {
            return 0;
        }
        i++;
    }

    return 1;
}

static const char *clboot_shell_skip_spaces(const char *text) {
    while (text != (const char *)0 && (*text == ' ' || *text == '\t')) {
        text++;
    }
    return text == (const char *)0 ? "" : text;
}

static void clboot_shell_print_ascii(const char *text) {
    CHAR16 ch[2];
    UINTN i = 0U;

    if (text == (const char *)0) {
        return;
    }

    while (text[i] != '\0') {
        if (text[i] == '\n') {
            clboot_print(L"\r\n");
        } else if (text[i] == '\r') {
            clboot_print(L"\r");
        } else {
            ch[0] = (CHAR16)(unsigned char)text[i];
            ch[1] = 0U;
            clboot_print(ch);
        }
        i++;
    }
}

static void clboot_shell_print_hex64(uint64_t value) {
    static const CHAR16 hex[] = L"0123456789ABCDEF";
    CHAR16 out[19];
    int i;

    out[0] = L'0';
    out[1] = L'x';
    for (i = 0; i < 16; i++) {
        out[2 + i] = hex[(value >> (uint64_t)((15 - i) * 4)) & 0xFULL];
    }
    out[18] = 0U;
    clboot_print(out);
}

static void clboot_shell_arg_to_path(const char *arg, CHAR16 *out_path, UINTN out_size) {
    UINTN pos = 0U;
    UINTN i = 0U;

    if (out_path == (CHAR16 *)0 || out_size == 0U) {
        return;
    }

    arg = clboot_shell_skip_spaces(arg);
    if (arg[0] == '\0') {
        out_path[0] = L'\\';
        out_path[1] = 0U;
        return;
    }

    if (arg[0] != '\\' && arg[0] != '/') {
        out_path[pos++] = L'\\';
    }

    while (pos + 1U < out_size && arg[i] != '\0' && arg[i] != ' ' && arg[i] != '\t') {
        out_path[pos++] = (arg[i] == '/') ? L'\\' : (CHAR16)(unsigned char)arg[i];
        i++;
    }
    out_path[pos] = 0U;
}

static int clboot_shell_read_line(char *out, UINTN out_size) {
    UINTN len = 0U;
    EFI_INPUT_KEY key;

    if (out == (char *)0 || out_size == 0U) {
        return 0;
    }

    out[0] = '\0';
    clboot_enable_cursor(1);

    for (;;) {
        while (clboot_poll_key(&key) == 0) {
            if (clboot_bs != (EFI_BOOT_SERVICES *)0 && clboot_bs->Stall != (void *)0) {
                (void)clboot_bs->Stall(10000U);
            }
        }

        if (key.UnicodeChar == L'\r' || key.UnicodeChar == L'\n') {
            out[len] = '\0';
            clboot_print(L"\r\n");
            return 1;
        }

        if (key.UnicodeChar == 8U || key.UnicodeChar == 127U) {
            if (len > 0U) {
                len--;
                out[len] = '\0';
                clboot_print(L"\b \b");
            }
            continue;
        }

        if (key.UnicodeChar >= 0x20U && key.UnicodeChar < 0x7FU && len + 1U < out_size) {
            CHAR16 ch[2];
            out[len++] = (char)(key.UnicodeChar & 0xFFU);
            out[len] = '\0';
            ch[0] = key.UnicodeChar;
            ch[1] = 0U;
            clboot_print(ch);
        }
    }
}

static void clboot_shell_help(void) {
    clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
    clboot_print(L"Commands\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    clboot_print(L"  help                 show this help\r\n");
    clboot_print(L"  ls [path]            list files on boot filesystem\r\n");
    clboot_print(L"  cat <path>           print a small text file preview\r\n");
    clboot_print(L"  info                 show boot entry and firmware info\r\n");
    clboot_print(L"  fb                   show framebuffer/GOP info\r\n");
    clboot_print(L"  memmap               show UEFI memory map summary\r\n");
    clboot_print(L"  fs                   show detected filesystem handles\r\n");
    clboot_print(L"  bootlog              show CLBoot log buffer\r\n");
    clboot_print(L"  clear                clear screen\r\n");
    clboot_print(L"  exit                 return to boot menu\r\n");
}

static void clboot_shell_ls(const char *arg) {
    CHAR16 path[CLBOOT_MENU_PATH_MAX];
    EFI_FILE_PROTOCOL *root;
    EFI_FILE_PROTOCOL *dir;
    EFI_FILE_INFO *info;
    UINTN entries = 0U;

    clboot_shell_arg_to_path(arg, path, sizeof(path) / sizeof(path[0]));
    root = clboot_open_root();
    if (root == (EFI_FILE_PROTOCOL *)0) {
        clboot_print(L"ls: cannot open boot filesystem\r\n");
        return;
    }

    if (path[0] == L'\\' && path[1] == 0U) {
        dir = root;
    } else if (root->Open(root, &dir, path, EFI_FILE_MODE_READ, 0ULL) != EFI_SUCCESS) {
        clboot_print(L"ls: cannot open ");
        clboot_print(path);
        clboot_print(L"\r\n");
        (void)root->Close(root);
        return;
    }

    if (clboot_alloc_pool(4096U, (void **)&info) == 0) {
        clboot_print(L"ls: out of memory\r\n");
        if (dir != root) {
            (void)dir->Close(dir);
        }
        (void)root->Close(root);
        return;
    }

    clboot_print(L"Directory: ");
    clboot_print(path);
    clboot_print(L"\r\n");

    for (;;) {
        EFI_STATUS status;
        UINTN size = 4096U;

        clboot_memset(info, 0, 4096U);
        status = dir->Read(dir, &size, info);
        if (EFI_ERROR(status) || size == 0U) {
            break;
        }

        clboot_print((info->Attribute & EFI_FILE_DIRECTORY) != 0ULL ? L"  <DIR> " : L"        ");
        clboot_print_uint((UINTN)info->FileSize);
        clboot_print(L"  ");
        clboot_print(info->FileName);
        clboot_print(L"\r\n");
        entries++;
    }

    clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
    clboot_print(L"entries: ");
    clboot_print_uint(entries);
    clboot_print(L"\r\n");
    clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);

    (void)clboot_bs->FreePool(info);
    if (dir != root) {
        (void)dir->Close(dir);
    }
    (void)root->Close(root);
}

static void clboot_shell_cat(const char *arg) {
    CHAR16 path[CLBOOT_MENU_PATH_MAX];
    struct clboot_loaded_file file;
    UINTN limit;
    UINTN i;

    clboot_shell_arg_to_path(arg, path, sizeof(path) / sizeof(path[0]));
    if (path[0] == L'\\' && path[1] == 0U) {
        clboot_print(L"cat: file path required\r\n");
        return;
    }

    if (clboot_read_file(path, &file) == 0) {
        clboot_print(L"cat: cannot read ");
        clboot_print(path);
        clboot_print(L"\r\n");
        return;
    }

    limit = (file.size > 4096ULL) ? 4096U : (UINTN)file.size;
    for (i = 0U; i < limit; i++) {
        char ch = ((const char *)file.data)[i];
        if (ch == '\n') {
            clboot_print(L"\r\n");
        } else if (ch == '\r') {
            clboot_print(L"\r");
        } else if ((unsigned char)ch >= 0x20U && (unsigned char)ch < 0x7FU) {
            CHAR16 out[2];
            out[0] = (CHAR16)(unsigned char)ch;
            out[1] = 0U;
            clboot_print(out);
        } else if (ch == '\t') {
            clboot_print(L"\t");
        } else {
            clboot_print(L".");
        }
    }
    if (file.size > (uint64_t)limit) {
        clboot_set_attr(CLBOOT_TEXT_ATTR_DIM);
        clboot_print(L"\r\n[truncated]\r\n");
        clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);
    } else {
        clboot_print(L"\r\n");
    }

    (void)clboot_bs->FreePool(file.data);
}

static void clboot_shell_fb(void) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = (EFI_GRAPHICS_OUTPUT_PROTOCOL *)0;

    if (clboot_bs->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, (void *)0, (void **)&gop) != EFI_SUCCESS ||
        gop == (EFI_GRAPHICS_OUTPUT_PROTOCOL *)0 || gop->Mode == (EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *)0 ||
        gop->Mode->Info == (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *)0) {
        clboot_print(L"fb: GOP not available\r\n");
        return;
    }

    clboot_print(L"Framebuffer base : ");
    clboot_shell_print_hex64(gop->Mode->FrameBufferBase);
    clboot_print(L"\r\nFramebuffer size : ");
    clboot_print_uint(gop->Mode->FrameBufferSize);
    clboot_print(L"\r\nResolution       : ");
    clboot_print_uint(gop->Mode->Info->HorizontalResolution);
    clboot_print(L"x");
    clboot_print_uint(gop->Mode->Info->VerticalResolution);
    clboot_print(L"\r\nPixels per line  : ");
    clboot_print_uint(gop->Mode->Info->PixelsPerScanLine);
    clboot_print(L"\r\nPixel format     : ");
    clboot_print_uint(gop->Mode->Info->PixelFormat);
    clboot_print(L"\r\n");
}

static void clboot_shell_fs(void) {
    EFI_HANDLE *handles = (EFI_HANDLE *)0;
    UINTN handle_count = 0U;

    if (clboot_bs->LocateHandleBuffer(EFI_BY_PROTOCOL, &gEfiSimpleFileSystemProtocolGuid, (void *)0, &handle_count,
                                      &handles) != EFI_SUCCESS ||
        handles == (EFI_HANDLE *)0) {
        clboot_print(L"filesystems: 0\r\n");
        return;
    }

    clboot_print(L"filesystems: ");
    clboot_print_uint(handle_count);
    clboot_print(L"\r\n");
    (void)clboot_bs->FreePool(handles);
}

static void clboot_shell_memmap(void) {
    EFI_MEMORY_DESCRIPTOR *mmap;
    UINTN mmap_size = 128U * 1024U;
    UINTN mmap_key = 0U;
    UINTN desc_size = 0U;
    UINT32 desc_version = 0U;
    UINTN count;
    UINTN i;
    uint64_t pages_total = 0ULL;
    uint64_t pages_usable = 0ULL;

    if (clboot_alloc_pool(mmap_size, (void **)&mmap) == 0) {
        clboot_print(L"memmap: out of memory\r\n");
        return;
    }

    if (clboot_bs->GetMemoryMap(&mmap_size, mmap, &mmap_key, &desc_size, &desc_version) != EFI_SUCCESS ||
        desc_size == 0U) {
        clboot_print(L"memmap: GetMemoryMap failed\r\n");
        (void)clboot_bs->FreePool(mmap);
        return;
    }

    count = mmap_size / desc_size;
    clboot_print(L"entries: ");
    clboot_print_uint(count);
    clboot_print(L"\r\n");
    clboot_print(L"first entries:\r\n");

    for (i = 0U; i < count; i++) {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)mmap + (i * desc_size));
        pages_total += desc->NumberOfPages;
        if (desc->Type == EFI_CONVENTIONAL_MEMORY) {
            pages_usable += desc->NumberOfPages;
        }
        if (i < 16U) {
            clboot_print(L"  type=");
            clboot_print_uint(desc->Type);
            clboot_print(L" base=");
            clboot_shell_print_hex64(desc->PhysicalStart);
            clboot_print(L" pages=");
            clboot_print_uint(desc->NumberOfPages);
            clboot_print(L"\r\n");
        }
    }

    clboot_print(L"total KiB : ");
    clboot_print_uint((UINTN)(pages_total * 4ULL));
    clboot_print(L"\r\nusable KiB: ");
    clboot_print_uint((UINTN)(pages_usable * 4ULL));
    clboot_print(L"\r\n");
    (void)clboot_bs->FreePool(mmap);
}

static void clboot_shell_info(const struct clboot_menu_config *cfg, UINTN selected) {
    if (cfg != (const struct clboot_menu_config *)0 && selected < cfg->count) {
        const struct clboot_menu_entry *entry = &cfg->entries[selected];
        clboot_print(L"Selected entry : ");
        clboot_print(entry->title);
        clboot_print(L"\r\nKernel         : ");
        clboot_print(entry->kernel_path);
        clboot_print(L"\r\nRamdisk        : ");
        clboot_print(entry->ramdisk_path);
        clboot_print(L"\r\nValid          : ");
        clboot_print(entry->valid != 0 ? L"yes" : L"no");
        clboot_print(L"\r\nCmdline        : ");
        clboot_shell_print_ascii(entry->cmdline[0] == '\0' ? "<empty>" : entry->cmdline);
        clboot_print(L"\r\n");
    }

    if (clboot_st != (EFI_SYSTEM_TABLE *)0) {
        clboot_print(L"UEFI revision  : ");
        clboot_shell_print_hex64(clboot_st->Hdr.Revision);
        clboot_print(L"\r\nText mode      : ");
        if (clboot_st->ConOut != (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *)0 &&
            clboot_st->ConOut->Mode != (SIMPLE_TEXT_OUTPUT_MODE *)0) {
            clboot_print_uint((UINTN)clboot_st->ConOut->Mode->Mode);
            clboot_print(L"\r\n");
        } else {
            clboot_print(L"unknown\r\n");
        }
    }
}

static void clboot_shell_bootlog(void) {
    UINTN i;

    for (i = 0U; i < clboot_bootlog_used; i++) {
        char ch = clboot_bootlog[i];
        if (ch == '\n') {
            clboot_print(L"\r\n");
        } else {
            CHAR16 out[2];
            out[0] = (CHAR16)(unsigned char)ch;
            out[1] = 0U;
            clboot_print(out);
        }
    }
    clboot_print(L"\r\n");
}

static void clboot_shell_run(const struct clboot_menu_config *cfg, UINTN selected) {
    char line[160];

    clboot_draw_compact_header(L"CLBoot shell");
    clboot_print(L"Type 'help' for commands, 'exit' to return to Boot Menu.\r\n");
    clboot_reset_input();

    for (;;) {
        const char *arg;

        clboot_set_attr(CLBOOT_TEXT_ATTR_ACCENT);
        clboot_print(L"clboot> ");
        clboot_set_attr(CLBOOT_TEXT_ATTR_NORMAL);

        if (clboot_shell_read_line(line, sizeof(line)) == 0) {
            continue;
        }

        arg = clboot_shell_skip_spaces(line);
        if (arg[0] == '\0') {
            continue;
        }

        if (clboot_shell_streq(arg, "exit") != 0 || clboot_shell_streq(arg, "quit") != 0) {
            clboot_enable_cursor(0);
            return;
        }
        if (clboot_shell_streq(arg, "help") != 0 || clboot_shell_streq(arg, "?") != 0) {
            clboot_shell_help();
        } else if (clboot_shell_streq(arg, "clear") != 0 || clboot_shell_streq(arg, "cls") != 0) {
            clboot_clear_screen();
        } else if (clboot_shell_streq(arg, "info") != 0) {
            clboot_shell_info(cfg, selected);
        } else if (clboot_shell_streq(arg, "fb") != 0) {
            clboot_shell_fb();
        } else if (clboot_shell_streq(arg, "fs") != 0) {
            clboot_shell_fs();
        } else if (clboot_shell_streq(arg, "memmap") != 0) {
            clboot_shell_memmap();
        } else if (clboot_shell_streq(arg, "bootlog") != 0) {
            clboot_shell_bootlog();
        } else if (clboot_shell_starts_with(arg, "ls") != 0 && (arg[2] == '\0' || arg[2] == ' ' || arg[2] == '\t')) {
            clboot_shell_ls(arg + 2);
        } else if (clboot_shell_starts_with(arg, "cat") != 0 &&
                   (arg[3] == '\0' || arg[3] == ' ' || arg[3] == '\t')) {
            clboot_shell_cat(arg + 3);
        } else {
            clboot_print(L"unknown command: ");
            clboot_shell_print_ascii(arg);
            clboot_print(L"\r\n");
        }
    }
}
