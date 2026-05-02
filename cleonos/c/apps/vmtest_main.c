#include "cmd_runtime.h"

static void vmtest_print_hex(const char *name, u64 value) {
    (void)printf("%s0x%llX\n", name, (unsigned long long)value);
}

int cleonos_app_main(int argc, char **argv, char **envp) {
    u64 size = 8ULL * 1024ULL * 1024ULL;
    unsigned char *mem;
    u64 i;
    u64 checksum = 0ULL;

    (void)envp;

    if (argc >= 2 && argv != (char **)0 && argv[1] != (char *)0) {
        u64 parsed = 0ULL;
        if (ush_parse_u64_dec(argv[1], &parsed) != 0 && parsed > 0ULL) {
            size = parsed;
        }
    }

    (void)printf("vmtest: alloc %llu bytes\n", (unsigned long long)size);
    mem = (unsigned char *)cleonos_sys_vm_alloc(size, CLEONOS_VM_FLAG_READ | CLEONOS_VM_FLAG_WRITE);
    if (mem == (unsigned char *)0) {
        (void)puts("vmtest: vm_alloc failed");
        return 1;
    }

    vmtest_print_hex("vmtest: ptr=", (u64)(usize)mem);

    for (i = 0ULL; i < size; i += 4096ULL) {
        mem[i] = (unsigned char)((i >> 12ULL) & 0xFFULL);
    }

    for (i = 0ULL; i < size; i += 4096ULL) {
        checksum += (u64)mem[i];
    }

    (void)printf("vmtest: checksum=0x%llX\n", (unsigned long long)checksum);

    if (cleonos_sys_vm_free(mem, size) == 0ULL) {
        (void)puts("vmtest: vm_free failed");
        return 1;
    }

    (void)puts("vmtest: ok");
    return 0;
}
