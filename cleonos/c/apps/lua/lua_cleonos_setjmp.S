.text
.global setjmp
.type setjmp, @function
setjmp:
    movq (%rsp), %rax
    movq %rax, 0(%rdi)
    leaq 8(%rsp), %rax
    movq %rax, 8(%rdi)
    movq %rbp, 16(%rdi)
    movq %rbx, 24(%rdi)
    movq %r12, 32(%rdi)
    movq %r13, 40(%rdi)
    movq %r14, 48(%rdi)
    movq %r15, 56(%rdi)
    xorl %eax, %eax
    ret

.global longjmp
.type longjmp, @function
longjmp:
    movl %esi, %eax
    testl %eax, %eax
    jne 1f
    movl $1, %eax
1:
    movq 16(%rdi), %rbp
    movq 24(%rdi), %rbx
    movq 32(%rdi), %r12
    movq 40(%rdi), %r13
    movq 48(%rdi), %r14
    movq 56(%rdi), %r15
    movq 8(%rdi), %rsp
    jmp *0(%rdi)
