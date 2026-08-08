.section .text
.code64

.global enter_user_mode
.type enter_user_mode, @function
enter_user_mode:
    mov $0x1B, %ax
    mov %ax, %ds
    mov %ax, %es

    pushq $0x1B
    push %rsi
    pushfq
    pop %rax
    or $0x200, %rax
    push %rax
    pushq $0x23
    push %rdi
    iretq

.size enter_user_mode, . - enter_user_mode

.section .usertext,"ax"
.global user_program_start
.type user_program_start, @function
user_program_start:
    mov $0, %rax
    mov $'U', %rdi
    int $0x80
    mov $0, %rax
    mov $'3', %rdi
    int $0x80
    mov $0, %rax
    mov $' ', %rdi
    int $0x80

1:
    pause
    jmp 1b

.size user_program_start, . - user_program_start

.section .note.GNU-stack,"",@progbits
