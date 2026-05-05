#ifndef CLEONOS_BDT_DLFCN_H
#define CLEONOS_BDT_DLFCN_H

#define RTLD_NOW 2
#define RTLD_GLOBAL 0x100

void *dlopen(const char *path, int flags);
void *dlsym(void *handle, const char *symbol);
int dlclose(void *handle);

#endif
