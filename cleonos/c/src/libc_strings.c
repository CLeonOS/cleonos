#include <strings.h>

#include <ctype.h>

static int clib_strings_lower(int ch) {
    if (ch >= 'A' && ch <= 'Z') {
        return ch - 'A' + 'a';
    }

    return ch;
}

__attribute__((weak)) int strcasecmp(const char *left, const char *right) {
    size_t i = 0U;

    if (left == right) {
        return 0;
    }

    if (left == (const char *)0) {
        return -1;
    }

    if (right == (const char *)0) {
        return 1;
    }

    while (left[i] != '\0' && right[i] != '\0') {
        int lc = clib_strings_lower((unsigned char)left[i]);
        int rc = clib_strings_lower((unsigned char)right[i]);

        if (lc != rc) {
            return (lc < rc) ? -1 : 1;
        }

        i++;
    }

    if (left[i] == right[i]) {
        return 0;
    }

    return (left[i] < right[i]) ? -1 : 1;
}

__attribute__((weak)) int strncasecmp(const char *left, const char *right, size_t size) {
    size_t i = 0U;

    if (size == 0U || left == right) {
        return 0;
    }

    if (left == (const char *)0) {
        return -1;
    }

    if (right == (const char *)0) {
        return 1;
    }

    while (i < size && left[i] != '\0' && right[i] != '\0') {
        int lc = clib_strings_lower((unsigned char)left[i]);
        int rc = clib_strings_lower((unsigned char)right[i]);

        if (lc != rc) {
            return (lc < rc) ? -1 : 1;
        }

        i++;
    }

    if (i == size) {
        return 0;
    }

    if (left[i] == right[i]) {
        return 0;
    }

    return (left[i] < right[i]) ? -1 : 1;
}
