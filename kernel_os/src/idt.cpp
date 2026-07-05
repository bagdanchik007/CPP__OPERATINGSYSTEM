#include "idt.h"

namespace {
    IdtEntry idt[256];
    IdtPointer idt_ptr;
}

extern "C" void idt_load(uint64_t idt_ptr_addr); // idt_load.S

extern "C" void idt_set_entry(uint8_t vector, void (*handler)(), uint8_t type_attr) {
    uint64_t addr = reinterpret_cast<uint64_t>(handler);

    idt[vector].offset_low  = addr & 0xFFFF;
    idt[vector].selector    = 0x08;       // Kernel-Code-Segment-Selektor
    idt[vector].ist         = 0;
    idt[vector].type_attr   = type_attr;
    idt[vector].offset_mid  = (addr >> 16) & 0xFFFF;
    idt[vector].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[vector].reserved    = 0;
}

// Deklarationen der Stub-Einstiegspunkte aus isr_stubs.S.
// Wir brauchen für jeden Vektor einen EIGENEN Stub, weil die CPU bei
// manchen Exceptions einen Error-Code pusht und bei anderen nicht -
// der Stack wäre sonst inkonsistent.
extern "C" {
    void isr0();  void isr1();  void isr2();  void isr3();  void isr4();
    void isr5();  void isr6();  void isr7();  void isr8();  void isr9();
    void isr10(); void isr11(); void isr12(); void isr13(); void isr14();
    void isr15(); void isr16(); void isr17(); void isr18(); void isr19();
    void isr20(); void isr21(); void isr22(); void isr23(); void isr24();
    void isr25(); void isr26(); void isr27(); void isr28(); void isr29();
    void isr30(); void isr31();

    void irq0();  // Timer - der für uns wichtigste
    void irq1();  // Tastatur (Platzhalter für später)
}

extern "C" void idt_init() {
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base  = reinterpret_cast<uint64_t>(&idt);

    // type_attr = 0x8E: Present, Ring0, 64-bit Interrupt Gate
    // (Interrupt Gate statt Trap Gate -> IF wird beim Eintritt automatisch
    // gecleart, verhindert Verschachtelung von Interrupts ohne "sti")
    const uint8_t GATE = 0x8E;

    void (*exception_handlers[32])() = {
        isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
        isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    };

    for (int i = 0; i < 32; i++) {
        idt_set_entry(i, exception_handlers[i], GATE);
    }

    // Hardware-IRQs (nach PIC-Remap auf 0x20+ gemappt, siehe pic.cpp)
    idt_set_entry(0x20, irq0, GATE); // Timer
    idt_set_entry(0x21, irq1, GATE); // Tastatur

    idt_load(reinterpret_cast<uint64_t>(&idt_ptr));
}
