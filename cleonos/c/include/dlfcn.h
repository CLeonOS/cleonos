#ifndef CLEONOS_LIBC_DLFCN_H
#define CLEONOS_LIBC_DLFCN_H

#define RTLD_LAZY 0x1
#define RTLD_NOW 0x2
#define RTLD_NODELETE 0x1000

void *dlopen(const char *path, int mode);
void *dlsym(void *handle, const char *symbol);
int dlclose(void *handle);
char *dlerror(void);

#endif
