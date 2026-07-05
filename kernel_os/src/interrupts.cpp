#include "interrupts.h"
#include "pic.h"
#include "scheduler.h"

namespace {
    volatile uint16_t* vga = reinterpret_cast<volatile uint16_t*>(0xB8000);

    void panic_print(const char* str, uint64_t code) {
        // Sehr simple Debug-Ausgabe: Exception-Name + Fehlercode als Hex,
        // direkt in die zweite Bildschirmzeile geschrieben.
        int col = 80; // Zeile 2 im 80-spaltigen Textmodus
        for (int i = 0; str[i] != '\0'; i++) {
            vga[col++] = (0x4F << 8) | str[i]; // weiß auf rot
        }
        vga[col++] = (0x4F << 8) | ' ';
        for (int shift = 60; shift >= 0; shift -= 4) {
            uint8_t nibble = (code >> shift) & 0xF;
            char c = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
            vga[col++] = (0x4F << 8) | c;
        }
    }

    const char* exception_names[32] = {
        "Divide by Zero", "Debug", "NMI", "Breakpoint",
        "Overflow", "Bound Range", "Invalid Opcode", "Device N/A",
        "Double Fault", "Coproc Segment", "Invalid TSS", "Segment Not Present",
        "Stack Fault", "General Protection", "Page Fault", "Reserved",
        "x87 FP Exception", "Alignment Check", "Machine Check", "SIMD FP",
        "Virtualization", "Control Protection", "Reserved", "Reserved",
        "Reserved", "Reserved", "Reserved", "Reserved",
        "Hypervisor Injection", "VMM Communication", "Security Exception", "Reserved"
    };
}

// --- CPU-Exceptions (Divide-by-Zero, Page Fault, General Protection, ...) ---
extern "C" void exception_handler(InterruptFrame* frame) {
    // Für den Anfang: alle Exceptions sind fatal -> Debug-Info ausgeben, anhalten.
    // Später: z.B. Page Fault (Vektor 14) separat behandeln für Demand Paging /
    // Copy-on-Write, statt immer abzustürzen.
    panic_print(exception_names[frame->interrupt_number], frame->error_code);

    asm volatile("cli");
    for (;;) {
        asm volatile("hlt");
    }
}

// --- Hardware-Interrupts (Timer, Tastatur, ...) ---
extern "C" void irq_handler(InterruptFrame* frame) {
    switch (frame->interrupt_number) {
        case 32: // Timer (IRQ0) -> genau HIER wird der Scheduler getriggert
            pic_send_eoi(0);
            scheduler_tick();
            break;

        case 33: // Tastatur (IRQ1) -> Platzhalter für späteren Treiber
            pic_send_eoi(1);
            break;

        default:
            pic_send_eoi(frame->interrupt_number - 32);
            break;
    }
}
