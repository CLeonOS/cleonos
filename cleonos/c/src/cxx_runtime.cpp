extern "C" void *malloc(unsigned long size);
extern "C" void free(void *ptr);
extern "C" void abort(void);

void *operator new(unsigned long size) {
    return malloc(size == 0UL ? 1UL : size);
}

void *operator new[](unsigned long size) {
    return malloc(size == 0UL ? 1UL : size);
}

void operator delete(void *ptr) noexcept {
    free(ptr);
}

void operator delete[](void *ptr) noexcept {
    free(ptr);
}

void operator delete(void *ptr, unsigned long) noexcept {
    free(ptr);
}

void operator delete[](void *ptr, unsigned long) noexcept {
    free(ptr);
}

extern "C" void __cxa_pure_virtual(void) {
    abort();
}

extern "C" int __cxa_atexit(void (*)(void *), void *, void *) {
    return 0;
}

extern "C" void __cxa_finalize(void *) {}

void *__dso_handle = 0;
