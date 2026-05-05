#include "tcc/tcc_cleonos_compat.h"

#define main cleonos_tcc_main
#include "../../third-party/tinycc/tcc.c"
#undef main

int cleonos_app_main(int argc, char **argv, char **envp) {
    char *static_argv[64];
    int i;

    (void)envp;
    (void)cleonos_tcc_stdio_init();

    if (argc > 0 && argc + 1 < (int)(sizeof(static_argv) / sizeof(static_argv[0]))) {
        static_argv[0] = argv[0];
        static_argv[1] = (char *)"-static";
        for (i = 1; i < argc; i++) {
            static_argv[i + 1] = argv[i];
        }
        static_argv[argc + 1] = (char *)0;
        return cleonos_tcc_main(argc + 1, static_argv);
    }

    return cleonos_tcc_main(argc, argv);
}
