#pragma once
#include "idt.h"

// ============================================================
// System calls via int 0x80
// ============================================================

constexpr uint8_t SYSCALL_VECTOR = 0x80;

enum SyscallNumber : uint64_t {
    SYSCALL_WRITE_CHAR = 0,
};

extern "C" {

void syscall_init();
void syscall_handler(InterruptFrame* frame);

}
