#include <sys/mman.h>

#include <cleonos_syscall.h>

void *mmap(void *addr, size_t length, int prot, int flags, int fd, long offset) {
    u64 vm_flags = 0ULL;
    void *ptr;

    if (addr != (void *)0 || length == 0U || fd != -1 || offset != 0L) {
        return MAP_FAILED;
    }

    if ((flags & MAP_ANONYMOUS) == 0 || (flags & MAP_PRIVATE) == 0) {
        return MAP_FAILED;
    }

    if ((prot & PROT_READ) != 0) {
        vm_flags |= CLEONOS_VM_FLAG_READ;
    }
    if ((prot & PROT_WRITE) != 0) {
        vm_flags |= CLEONOS_VM_FLAG_WRITE;
    }
    if ((prot & PROT_EXEC) != 0) {
        vm_flags |= CLEONOS_VM_FLAG_EXEC;
    }

    ptr = cleonos_sys_vm_alloc((u64)length, vm_flags);
    return (ptr != (void *)0) ? ptr : MAP_FAILED;
}

int munmap(void *addr, size_t length) {
    if (addr == (void *)0 || length == 0U) {
        return -1;
    }

    return (cleonos_sys_vm_free(addr, (u64)length) != 0ULL) ? 0 : -1;
}
