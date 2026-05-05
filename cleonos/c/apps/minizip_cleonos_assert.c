#include <stdio.h>
#include <stdlib.h>

void __assert_fail(const char *expr, const char *file, unsigned int line, const char *func) {
    printf("assert failed: %s at %s:%u %s\n", expr ? expr : "?", file ? file : "?", line, func ? func : "?");
    abort();
}
