#include "bdt/bdt_cleonos_compat.h"

#define main cleonos_bdt_main
#include "../../../bdt/src/main.c"
#undef main

int cleonos_app_main(int argc, char **argv, char **envp) {
    bdt_cleonos_init();
    bdt_cleonos_import_env(envp);
    return cleonos_bdt_main(argc, argv);
}
