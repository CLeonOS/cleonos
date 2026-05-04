#include <setjmp.h>

__attribute__((naked)) int setjmp(jmp_buf env) {
    (void)env;
    __asm__ volatile(
        "movq (%rsp), %rax\n"
        "movq %rax, 0(%rdi)\n"
        "leaq 8(%rsp), %rax\n"
        "movq %rax, 8(%rdi)\n"
        "movq %rbp, 16(%rdi)\n"
        "movq %rbx, 24(%rdi)\n"
        "movq %r12, 32(%rdi)\n"
        "movq %r13, 40(%rdi)\n"
        "movq %r14, 48(%rdi)\n"
        "movq %r15, 56(%rdi)\n"
        "xorl %eax, %eax\n"
        "ret\n");
}

__attribute__((naked, noreturn)) void longjmp(jmp_buf env, int value) {
    (void)env;
    (void)value;
    __asm__ volatile(
        "movl %esi, %eax\n"
        "testl %eax, %eax\n"
        "jne 1f\n"
        "movl $1, %eax\n"
        "1:\n"
        "movq 16(%rdi), %rbp\n"
        "movq 24(%rdi), %rbx\n"
        "movq 32(%rdi), %r12\n"
        "movq 40(%rdi), %r13\n"
        "movq 48(%rdi), %r14\n"
        "movq 56(%rdi), %r15\n"
        "movq 8(%rdi), %rsp\n"
        "jmp *0(%rdi)\n");
}
