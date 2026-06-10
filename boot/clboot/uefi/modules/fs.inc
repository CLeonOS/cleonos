static int clboot_alloc_pool(UINTN size, void **out) {
    if (clboot_bs->AllocatePool(EFI_LOADER_DATA, size, out) != EFI_SUCCESS) {
        return 0;
    }
    clboot_memset(*out, 0, size);
    return 1;
}

static int clboot_alloc_pages(UINTN pages, UINT64 *out_addr) {
    UINT64 addr = 0ULL;
    if (clboot_bs->AllocatePages(EFI_ALLOCATE_ANY_PAGES, EFI_LOADER_DATA, pages, &addr) != EFI_SUCCESS) {
        return 0;
    }
    clboot_memset((void *)(UINTN)addr, 0, pages << 12U);
    *out_addr = addr;
    return 1;
}

static int clboot_alloc_pages_below(UINTN pages, UINT64 max_addr, UINT64 *out_addr) {
    UINT64 addr = max_addr;
    if (clboot_bs->AllocatePages(EFI_ALLOCATE_MAX_ADDRESS, EFI_LOADER_DATA, pages, &addr) != EFI_SUCCESS) {
        return 0;
    }
    clboot_memset((void *)(UINTN)addr, 0, pages << 12U);
    *out_addr = addr;
    return 1;
}

static EFI_FILE_PROTOCOL *clboot_open_root(void) {
    EFI_LOADED_IMAGE_PROTOCOL *loaded = (EFI_LOADED_IMAGE_PROTOCOL *)0;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = (EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *)0;
    EFI_FILE_PROTOCOL *root = (EFI_FILE_PROTOCOL *)0;

    if (clboot_bs->OpenProtocol(clboot_image, &gEfiLoadedImageProtocolGuid, (void **)&loaded, clboot_image,
                                (EFI_HANDLE)0, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL) != EFI_SUCCESS) {
        return (EFI_FILE_PROTOCOL *)0;
    }

    if (clboot_bs->OpenProtocol(loaded->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void **)&fs, clboot_image,
                                (EFI_HANDLE)0, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL) != EFI_SUCCESS) {
        return (EFI_FILE_PROTOCOL *)0;
    }

    if (fs->OpenVolume(fs, &root) != EFI_SUCCESS) {
        return (EFI_FILE_PROTOCOL *)0;
    }

    return root;
}

static int clboot_read_file_from_root(EFI_FILE_PROTOCOL *root, CHAR16 *path, struct clboot_loaded_file *out) {
    EFI_FILE_PROTOCOL *file;
    UINTN cap = 4096U;
    UINTN used = 0U;
    UINT8 *buf;

    out->data = (void *)0;
    out->size = 0ULL;
    if (root == (EFI_FILE_PROTOCOL *)0) {
        return 0;
    }
    if (root->Open(root, &file, path, EFI_FILE_MODE_READ, 0ULL) != EFI_SUCCESS) {
        return 0;
    }
    if (clboot_alloc_pool(cap, (void **)&buf) == 0) {
        (void)file->Close(file);
        return 0;
    }

    for (;;) {
        UINTN got = 4096U;
        if (used + got > cap) {
            UINT8 *new_buf;
            UINTN new_cap = cap * 2U;
            if (clboot_alloc_pool(new_cap, (void **)&new_buf) == 0) {
                (void)file->Close(file);
                return 0;
            }
            clboot_memcpy(new_buf, buf, used);
            (void)clboot_bs->FreePool(buf);
            buf = new_buf;
            cap = new_cap;
        }
        if (file->Read(file, &got, buf + used) != EFI_SUCCESS) {
            (void)file->Close(file);
            return 0;
        }
        if (got == 0U) {
            break;
        }
        used += got;
    }

    (void)file->Close(file);
    out->data = buf;
    out->size = (uint64_t)used;
    return 1;
}

static int clboot_read_file(CHAR16 *path, struct clboot_loaded_file *out) {
    EFI_FILE_PROTOCOL *root;
    EFI_HANDLE *handles;
    UINTN handle_count;
    UINTN i;

    root = clboot_open_root();
    if (root != (EFI_FILE_PROTOCOL *)0) {
        if (clboot_read_file_from_root(root, path, out) != 0) {
            (void)root->Close(root);
            return 1;
        }
        (void)root->Close(root);
    }

    handles = (EFI_HANDLE *)0;
    handle_count = 0U;
    if (clboot_bs->LocateHandleBuffer(EFI_BY_PROTOCOL, &gEfiSimpleFileSystemProtocolGuid, (void *)0, &handle_count,
                                      &handles) != EFI_SUCCESS ||
        handles == (EFI_HANDLE *)0) {
        return 0;
    }

    for (i = 0U; i < handle_count; i++) {
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = (EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *)0;
        EFI_FILE_PROTOCOL *scan_root = (EFI_FILE_PROTOCOL *)0;
        if (clboot_bs->OpenProtocol(handles[i], &gEfiSimpleFileSystemProtocolGuid, (void **)&fs, clboot_image,
                                    (EFI_HANDLE)0, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL) != EFI_SUCCESS ||
            fs == (EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *)0) {
            continue;
        }
        if (fs->OpenVolume(fs, &scan_root) != EFI_SUCCESS || scan_root == (EFI_FILE_PROTOCOL *)0) {
            continue;
        }
        if (clboot_read_file_from_root(scan_root, path, out) != 0) {
            (void)scan_root->Close(scan_root);
            (void)clboot_bs->FreePool(handles);
            return 1;
        }
        (void)scan_root->Close(scan_root);
    }

    (void)clboot_bs->FreePool(handles);
    return 0;
}

static int clboot_path_is_empty_or_none(const CHAR16 *path) {
    if (path == (const CHAR16 *)0 || path[0] == 0) {
        return 1;
    }

    if ((path[0] == L'n' || path[0] == L'N') && (path[1] == L'o' || path[1] == L'O') &&
        (path[2] == L'n' || path[2] == L'N') && (path[3] == L'e' || path[3] == L'E') && path[4] == 0) {
        return 1;
    }

    return 0;
}

static int clboot_file_exists(CHAR16 *path) {
    EFI_FILE_PROTOCOL *root;
    EFI_FILE_PROTOCOL *file;
    EFI_HANDLE *handles;
    UINTN handle_count;
    UINTN i;
    int ok = 0;

    root = clboot_open_root();
    if (root != (EFI_FILE_PROTOCOL *)0) {
        if (root->Open(root, &file, path, EFI_FILE_MODE_READ, 0ULL) == EFI_SUCCESS) {
            ok = 1;
            (void)file->Close(file);
        }
        (void)root->Close(root);
        if (ok != 0) {
            return 1;
        }
    }

    handles = (EFI_HANDLE *)0;
    handle_count = 0U;
    if (clboot_bs->LocateHandleBuffer(EFI_BY_PROTOCOL, &gEfiSimpleFileSystemProtocolGuid, (void *)0, &handle_count,
                                      &handles) != EFI_SUCCESS ||
        handles == (EFI_HANDLE *)0) {
        return 0;
    }

    for (i = 0U; i < handle_count; i++) {
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = (EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *)0;
        EFI_FILE_PROTOCOL *scan_root = (EFI_FILE_PROTOCOL *)0;
        if (clboot_bs->OpenProtocol(handles[i], &gEfiSimpleFileSystemProtocolGuid, (void **)&fs, clboot_image,
                                    (EFI_HANDLE)0, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL) != EFI_SUCCESS ||
            fs == (EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *)0) {
            continue;
        }
        if (fs->OpenVolume(fs, &scan_root) != EFI_SUCCESS || scan_root == (EFI_FILE_PROTOCOL *)0) {
            continue;
        }
        if (scan_root->Open(scan_root, &file, path, EFI_FILE_MODE_READ, 0ULL) == EFI_SUCCESS) {
            ok = 1;
            (void)file->Close(file);
            (void)scan_root->Close(scan_root);
            break;
        }
        (void)scan_root->Close(scan_root);
    }

    (void)clboot_bs->FreePool(handles);
    return ok;
}

static int clboot_read_file_any(CHAR16 **paths, struct clboot_loaded_file *out) {
    UINTN i = 0U;
    while (paths[i] != (CHAR16 *)0) {
        if (clboot_read_file(paths[i], out) != 0) {
            return 1;
        }
        i++;
    }
    return 0;
}

static void clboot_connect_all_controllers(void) {
    EFI_HANDLE *handles;
    UINTN handle_count;
    UINTN i;

    handles = (EFI_HANDLE *)0;
    handle_count = 0U;
    if (clboot_bs->LocateHandleBuffer(EFI_ALL_HANDLES, (EFI_GUID *)0, (void *)0, &handle_count, &handles) !=
            EFI_SUCCESS ||
        handles == (EFI_HANDLE *)0) {
        return;
    }

    for (i = 0U; i < handle_count; i++) {
        (void)clboot_bs->ConnectController(handles[i], (EFI_HANDLE *)0, (void *)0, 1U);
    }

    (void)clboot_bs->FreePool(handles);
}
