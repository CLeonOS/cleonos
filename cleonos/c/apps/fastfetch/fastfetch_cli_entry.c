#define main fastfetch_upstream_main
#include "../../../third-party/fastfetch/src/fastfetch.c"
#undef main

static void fastfetch_cleonos_restore_tty(void) {
    if (instance.state.logoHeight > 0 && instance.state.keysHeight <= instance.state.logoHeight) {
        ffLogoPrintRemaining();
    }
    (void)fputs("\033[?25h\033[?7h\033[0m", stdout);
    (void)fflush(stdout);
}

static int fastfetch_cleonos_filter_args(int argc, char **argv, char **filtered, int max_filtered) {
    int out = 0;
    int i;

    for (i = 0; i < argc && out < max_filtered - 1; i++) {
        if (argv[i] != (char *)0 && strcmp(argv[i], "--dynamic-interval") == 0) {
            if (i + 1 < argc) {
                i++;
            }
            continue;
        }
        filtered[out++] = argv[i];
    }

    filtered[out] = (char *)0;
    return out;
}

int fastfetch_cli_main(int argc, char **argv) {
    char *filtered_argv[64];
    int filtered_argc;

    filtered_argc = fastfetch_cleonos_filter_args(argc, argv, filtered_argv, 64);
    if (setjmp(ff_cl_exit_jmp) == 0) {
        (void)fastfetch_upstream_main(filtered_argc, filtered_argv);
        ff_cl_exit_status = 0;
    }

    fastfetch_cleonos_restore_tty();
    return 0;
}
