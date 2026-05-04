#ifndef CLEONOS_LUA_SETJMP_H
#define CLEONOS_LUA_SETJMP_H

typedef struct cleonos_lua_jmp_buf {
    void *rip;
    void *rsp;
    void *rbp;
    void *rbx;
    void *r12;
    void *r13;
    void *r14;
    void *r15;
} jmp_buf[1];

int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int value) __attribute__((noreturn));

#endif
