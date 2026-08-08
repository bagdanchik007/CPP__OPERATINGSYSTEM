#include "syscall.h"
#include "console.h"
#include "idt.h"

extern "C" void syscall_stub();

extern "C" void syscall_init() {
    // Present, DPL 3, 64-bit interrupt gate: ring-3 code may invoke int 0x80.
    idt_set_entry(SYSCALL_VECTOR, syscall_stub, 0xEE);
}

extern "C" void syscall_handler(InterruptFrame* frame) {
    switch (frame->rax) {
        case SYSCALL_WRITE_CHAR:
            console_put_char(static_cast<char>(frame->rdi));
            frame->rax = 0;
            break;

        default:
            frame->rax = static_cast<uint64_t>(-1);
            break;
    }
}
