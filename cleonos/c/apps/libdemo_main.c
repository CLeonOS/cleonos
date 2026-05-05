#include <stdio.h>
#include "cmd_runtime.h"

typedef unsigned long long u64;

u64 cleonos_libdemo_add(u64 left, u64 right) {
    return left + right;
}

u64 cleonos_libdemo_mul(u64 left, u64 right) {
    return left * right;
}

u64 cleonos_libdemo_hello(void) {
    ush_writeln_i18n("[libdemo] hello from libdemo.elf", "[libdemo] 来自 libdemo.elf 的问候");
    return 0ULL;
}

int cleonos_app_main(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;
    (void)envp;
    ush_writeln_i18n("[libdemo] dynamic library image ready", "[libdemo] 动态库镜像就绪");
    return 0;
}
