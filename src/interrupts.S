.section .text
.extern arch_interrupt_dispatch
.global isr_common
isr_common:
    cld
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rbp
    pushq %rdi
    pushq %rsi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq %rsp, %rdi
    call arch_interrupt_dispatch
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rsi
    popq %rdi
    popq %rbp
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    addq $16, %rsp
    iretq

.extern ap_idle_c
.global ap_entry_trampoline
ap_entry_trampoline:
    movq 24(%rdi),%rsp
    andq $-16,%rsp
    call ap_idle_c
1:  cli
    hlt
    jmp 1b

.macro ISR_NOERR n
.global isr_\n
isr_\n: pushq $0; pushq $\n; jmp isr_common
.endm
.macro ISR_ERR n
.global isr_\n
isr_\n: pushq $\n; jmp isr_common
.endm
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR 8
ISR_NOERR 9
ISR_ERR 10
ISR_ERR 11
ISR_ERR 12
ISR_ERR 13
ISR_ERR 14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR 17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR 29
ISR_ERR 30
ISR_NOERR 31
ISR_NOERR 32
ISR_NOERR 33
ISR_NOERR 128
ISR_NOERR 255

.section .bss
.align 8
user_kernel_rsp: .quad 0
user_kernel_cr3: .quad 0

.section .text
.global arch_enter_user
arch_enter_user:
    movq %rsp,user_kernel_rsp(%rip)
    movq %cr3,%rax
    movq %rax,user_kernel_cr3(%rip)
    movq %rdx,%cr3
    movw $0x2b,%ax
    movw %ax,%ds
    movw %ax,%es
    pushq $0x2b
    pushq %rsi
    pushfq
    orq $0x200,(%rsp)
    pushq $0x33
    pushq %rdi
    iretq

.global arch_return_from_user
arch_return_from_user:
    movq %rdi,%rax
    movq user_kernel_cr3(%rip),%rcx
    movq %rcx,%cr3
    movq user_kernel_rsp(%rip),%rsp
    movw $0x10,%dx
    movw %dx,%ds
    movw %dx,%es
    ret

.section .rodata
.global isr_stub_table
isr_stub_table:
.quad isr_0,isr_1,isr_2,isr_3,isr_4,isr_5,isr_6,isr_7
.quad isr_8,isr_9,isr_10,isr_11,isr_12,isr_13,isr_14,isr_15
.quad isr_16,isr_17,isr_18,isr_19,isr_20,isr_21,isr_22,isr_23
.quad isr_24,isr_25,isr_26,isr_27,isr_28,isr_29,isr_30,isr_31
.quad isr_32,isr_33
.section .note.GNU-stack,"",@progbits
