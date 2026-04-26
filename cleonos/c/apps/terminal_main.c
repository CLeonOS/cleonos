#include "terminal/terminal.h"

int cleonos_app_main(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;
    (void)envp;

    return (cleonos_terminal_run() != 0) ? 0 : 1;
}
