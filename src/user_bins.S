.section .rodata
.global user_init_start,user_init_end,user_shell_start,user_shell_end,user_hello_start,user_hello_end,user_loggerd_start,user_loggerd_end,user_deviced_start,user_deviced_end
user_init_start:
.incbin "build/user/init.elf"
user_init_end:
user_shell_start:
.incbin "build/user/shell.elf"
user_shell_end:
user_hello_start:
.incbin "build/user/hello.elf"
user_hello_end:
user_loggerd_start:
.incbin "build/user/loggerd.elf"
user_loggerd_end:
user_deviced_start:
.incbin "build/user/deviced.elf"
user_deviced_end:
.section .note.GNU-stack,"",@progbits
