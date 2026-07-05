#pragma once
#include <stdint.h>

// ============================================================
// Interrupt Descriptor Table (IDT)
// ============================================================

struct __attribute__((packed)) IdtEntry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;         // Interrupt Stack Table Index (0 = nicht genutzt)
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
};

struct __attribute__((packed)) IdtPointer {
    uint16_t limit;
    uint64_t base;
};

// CPU-Register-Zustand, der von den Interrupt-Stubs (isr_stubs.S) auf
// den Stack gelegt wird, bevor der C++-Handler aufgerufen wird.
struct InterruptFrame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t interrupt_number, error_code;   // von den Stubs gepusht
    uint64_t rip, cs, rflags, rsp, ss;        // automatisch von der CPU gepusht
};

extern "C" {

void idt_init();

// Registriert einen Handler für eine bestimmte Interrupt-Nummer.
// Wird von idt_init() für die Standard-Handler benutzt, kann aber auch
// von außen genutzt werden (z.B. für Syscalls auf Vektor 0x80).
void idt_set_entry(uint8_t vector, void (*handler)(), uint8_t type_attr);

}
