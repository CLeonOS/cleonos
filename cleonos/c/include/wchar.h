#ifndef CLEONOS_LIBC_WCHAR_H
#define CLEONOS_LIBC_WCHAR_H

#include <stddef.h>

typedef int wchar_t;
typedef struct {
    int count;
} mbstate_t;

size_t mbrtowc(wchar_t *out, const char *s, size_t n, mbstate_t *ps);

#endif
