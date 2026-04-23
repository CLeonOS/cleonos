#include <cleonos_syscall.h>

extern int main(int argc, char **argv);

int cleonos_app_main(int argc, char **argv, char **envp) {
    (void)envp;
    return main(argc, argv);
}
