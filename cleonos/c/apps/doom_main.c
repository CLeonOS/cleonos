#include <stdio.h>
#include <string.h>

#include <cleonos_syscall.h>

int cl_doom_run_main(int argc, char **argv);

static const char *cl_doom_pick_default_wad(void) {
    static const char *candidates[] = {
        "/doom1.wad",
        "/DOOM1.WAD",
        "/temp/doom1.wad",
        "/temp/DOOM1.WAD",
        "/shell/doom1.wad",
        "/shell/DOOM1.WAD",
    };
    u64 i;

    for (i = 0ULL; i < (u64)(sizeof(candidates) / sizeof(candidates[0])); i++) {
        if (cleonos_sys_fs_stat_size(candidates[i]) != (u64)-1) {
            return candidates[i];
        }
    }

    return (const char *)0;
}

int cleonos_app_main(int argc, char **argv, char **envp) {
    char *run_argv[32];
    int run_argc = 0;
    int i;
    const char *auto_wad = (const char *)0;
    int has_iwad = 0;

    (void)envp;

    if (argc <= 0 || argv == (char **)0 || argv[0] == (char *)0) {
        (void)printf("doom: invalid argv\n");
        return 1;
    }

    for (i = 1; i < argc; i++) {
        if (argv[i] != (char *)0 && strcmp(argv[i], "-iwad") == 0) {
            has_iwad = 1;
            break;
        }
    }

    run_argv[run_argc++] = argv[0];

    if (has_iwad == 0) {
        if (argc > 1 && argv[1] != (char *)0 && argv[1][0] != '-') {
            run_argv[run_argc++] = "-iwad";
            run_argv[run_argc++] = argv[1];
            for (i = 2; i < argc && run_argc + 1 < (int)(sizeof(run_argv) / sizeof(run_argv[0])); i++) {
                run_argv[run_argc++] = argv[i];
            }
        } else {
            auto_wad = cl_doom_pick_default_wad();
            if (auto_wad != (const char *)0) {
                run_argv[run_argc++] = "-iwad";
                run_argv[run_argc++] = (char *)auto_wad;
            }

            for (i = 1; i < argc && run_argc + 1 < (int)(sizeof(run_argv) / sizeof(run_argv[0])); i++) {
                run_argv[run_argc++] = argv[i];
            }
        }
    } else {
        for (i = 1; i < argc && run_argc + 1 < (int)(sizeof(run_argv) / sizeof(run_argv[0])); i++) {
            run_argv[run_argc++] = argv[i];
        }
    }

    run_argv[run_argc] = (char *)0;

    if (has_iwad == 0 && auto_wad == (const char *)0 && (argc <= 1 || argv[1] == (char *)0 || argv[1][0] == '-')) {
        (void)printf("doom: no WAD provided\n");
        (void)printf("doom: usage: doom /doom1.wad\n");
        (void)printf("doom: or:    doom -iwad /doom1.wad\n");
        return 1;
    }

    (void)printf("doom: launching doomgeneric\n");
    return cl_doom_run_main(run_argc, run_argv);
}

