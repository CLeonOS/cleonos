static void *clboot_memset(void *dst, int value, UINTN size) {
    UINT8 *p = (UINT8 *)dst;
    while (size-- != 0U) {
        *p++ = (UINT8)value;
    }
    return dst;
}

static void *clboot_memcpy(void *dst, const void *src, UINTN size) {
    UINT8 *d = (UINT8 *)dst;
    const UINT8 *s = (const UINT8 *)src;
    while (size-- != 0U) {
        *d++ = *s++;
    }
    return dst;
}

static UINTN clboot_strlen(const char *text) {
    UINTN len = 0U;
    if (text == (const char *)0) {
        return 0U;
    }
    while (text[len] != '\0') {
        len++;
    }
    return len;
}

static int clboot_str_equal_n(const char *left, const char *right, UINTN len) {
    UINTN i;

    if (left == (const char *)0 || right == (const char *)0) {
        return 0;
    }
    for (i = 0U; i < len; i++) {
        if (left[i] != right[i]) {
            return 0;
        }
    }
    return right[len] == '\0' ? 1 : 0;
}

static void clboot_copy_ascii(char *dst, UINTN dst_size, const char *src) {
    UINTN i = 0U;

    if (dst == (char *)0 || dst_size == 0U) {
        return;
    }
    if (src != (const char *)0) {
        while (i + 1U < dst_size && src[i] != '\0') {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
}

static void clboot_copy_ascii_n(char *dst, UINTN dst_size, const char *src, UINTN src_len) {
    UINTN i = 0U;

    if (dst == (char *)0 || dst_size == 0U) {
        return;
    }
    if (src != (const char *)0) {
        while (i + 1U < dst_size && i < src_len && src[i] != '\0') {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
}

static UINTN clboot_wstrlen(const CHAR16 *text) {
    UINTN len = 0U;
    if (text == (CHAR16 *)0) {
        return 0U;
    }
    while (text[len] != 0U) {
        len++;
    }
    return len;
}

static void clboot_copy_wide(CHAR16 *dst, UINTN dst_size, CHAR16 *src) {
    UINTN i = 0U;

    if (dst == (CHAR16 *)0 || dst_size == 0U) {
        return;
    }
    if (src != (CHAR16 *)0) {
        while (i + 1U < dst_size && src[i] != 0U) {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = 0U;
}

static void clboot_ascii_to_wide(CHAR16 *dst, UINTN dst_size, const char *src) {
    UINTN i = 0U;

    if (dst == (CHAR16 *)0 || dst_size == 0U) {
        return;
    }
    if (src != (const char *)0) {
        while (i + 1U < dst_size && src[i] != '\0') {
            dst[i] = (CHAR16)(unsigned char)src[i];
            i++;
        }
    }
    dst[i] = 0U;
}

static void clboot_ascii_n_to_wide(CHAR16 *dst, UINTN dst_size, const char *src, UINTN src_len) {
    UINTN i = 0U;

    if (dst == (CHAR16 *)0 || dst_size == 0U) {
        return;
    }
    if (src != (const char *)0) {
        while (i + 1U < dst_size && i < src_len && src[i] != '\0') {
            dst[i] = (CHAR16)(unsigned char)src[i];
            i++;
        }
    }
    dst[i] = 0U;
}

static void clboot_print_repeat(CHAR16 ch, UINTN count) {
    CHAR16 text[2];
    text[0] = ch;
    text[1] = 0U;

    while (count-- != 0U) {
        clboot_print(text);
    }
}

static void clboot_print_padded(const CHAR16 *text, UINTN width) {
    UINTN len = clboot_wstrlen(text);

    if (text != (CHAR16 *)0) {
        clboot_print(text);
    }
    if (len < width) {
        clboot_print_repeat(L' ', width - len);
    }
}

static void clboot_print_uint(UINTN value) {
    CHAR16 digits[24];
    CHAR16 out[24];
    UINTN count = 0U;
    UINTN i;

    if (value == 0U) {
        clboot_print(L"0");
        return;
    }

    while (value != 0U && count < (sizeof(digits) / sizeof(digits[0]))) {
        digits[count++] = (CHAR16)(L'0' + (value % 10U));
        value /= 10U;
    }

    for (i = 0U; i < count; i++) {
        out[i] = digits[count - i - 1U];
    }
    out[count] = 0U;
    clboot_print(out);
}
