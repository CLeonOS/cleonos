#include "cmd_runtime.h"

int cleonos_app_main(int argc, char **argv, char **envp) {
    int fastfetch_cli_main(int argc, char **argv);

    cleonos_cmd_runtime_pre_main(envp);
    return fastfetch_cli_main(argc, argv);
}
