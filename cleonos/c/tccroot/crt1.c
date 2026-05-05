#include <cleonos_syscall.h>

#define CLEONOS_TCC_ARGV_MAX 24ULL
#define CLEONOS_TCC_ENVP_MAX 24ULL
#define CLEONOS_TCC_ITEM_MAX 128ULL

extern int main(int argc, char **argv, char **envp);

u64 _start(void) {
    static char argv_items[CLEONOS_TCC_ARGV_MAX][CLEONOS_TCC_ITEM_MAX];
    static char env_items[CLEONOS_TCC_ENVP_MAX][CLEONOS_TCC_ITEM_MAX];
    static char *argv_ptrs[CLEONOS_TCC_ARGV_MAX + 1ULL];
    static char *env_ptrs[CLEONOS_TCC_ENVP_MAX + 1ULL];
    u64 argc = cleonos_sys_proc_argc();
    u64 envc = cleonos_sys_proc_envc();
    u64 i;
    int code;

    if (argc > CLEONOS_TCC_ARGV_MAX) {
        argc = CLEONOS_TCC_ARGV_MAX;
    }
    if (envc > CLEONOS_TCC_ENVP_MAX) {
        envc = CLEONOS_TCC_ENVP_MAX;
    }

    for (i = 0ULL; i < argc; i++) {
        argv_items[i][0] = '\0';
        (void)cleonos_sys_proc_argv(i, argv_items[i], CLEONOS_TCC_ITEM_MAX);
        argv_ptrs[i] = argv_items[i];
    }
    argv_ptrs[argc] = (char *)0;

    for (i = 0ULL; i < envc; i++) {
        env_items[i][0] = '\0';
        (void)cleonos_sys_proc_env(i, env_items[i], CLEONOS_TCC_ITEM_MAX);
        env_ptrs[i] = env_items[i];
    }
    env_ptrs[envc] = (char *)0;

    code = main((int)argc, argv_ptrs, env_ptrs);
    return (u64)(unsigned int)code;
}
