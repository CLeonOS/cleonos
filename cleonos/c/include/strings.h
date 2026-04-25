#ifndef CLEONOS_LIBC_STRINGS_H
#define CLEONOS_LIBC_STRINGS_H

#include <stddef.h>

int strcasecmp(const char *left, const char *right);
int strncasecmp(const char *left, const char *right, size_t size);

#endif
