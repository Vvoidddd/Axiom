.section .text
.global _start
.extern main
_start:
    xorq %rbp,%rbp
    andq $-16,%rsp
    call main
    movq %rax,%rdi
    movq $5,%rax
    int $0x80
1:  jmp 1b
.section .note.GNU-stack,"",@progbits
