#include <sys/mman.h>

#include <cleonos_syscall.h>

void *mmap(void *addr, size_t length, int prot, int flags, int fd, long offset) {
    cleonos_mmap_req req;
    void *ptr;

    if (length == 0U || offset < 0L) {
        return MAP_FAILED;
    }

    if ((flags & MAP_PRIVATE) == 0 || (flags & MAP_SHARED) != 0 || (flags & MAP_FIXED) != 0) {
        return MAP_FAILED;
    }

    if ((flags & MAP_ANONYMOUS) != 0 && (fd != -1 || offset != 0L)) {
        return MAP_FAILED;
    }

    req.addr_hint = (u64)(usize)addr;
    req.length = (u64)length;
    req.prot = 0ULL;
    req.flags = 0ULL;
    req.fd = (fd < 0) ? (u64)-1 : (u64)fd;
    req.offset = (u64)offset;

    if ((prot & PROT_READ) != 0) {
        req.prot |= CLEONOS_VM_FLAG_READ;
    }
    if ((prot & PROT_WRITE) != 0) {
        req.prot |= CLEONOS_VM_FLAG_WRITE;
    }
    if ((prot & PROT_EXEC) != 0) {
        req.prot |= CLEONOS_VM_FLAG_EXEC;
    }

    if ((flags & MAP_PRIVATE) != 0) {
        req.flags |= CLEONOS_MMAP_FLAG_PRIVATE;
    }
    if ((flags & MAP_ANONYMOUS) != 0) {
        req.flags |= CLEONOS_MMAP_FLAG_ANONYMOUS;
    }

    ptr = cleonos_sys_mmap(&req);
    return (ptr != (void *)0) ? ptr : MAP_FAILED;
}

int munmap(void *addr, size_t length) {
    if (addr == (void *)0 || length == 0U) {
        return -1;
    }

    return (cleonos_sys_vm_free(addr, (u64)length) != 0ULL) ? 0 : -1;
}
