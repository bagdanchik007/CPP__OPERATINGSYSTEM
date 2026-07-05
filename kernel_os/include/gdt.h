#pragma once
#include <stdint.h>

// ============================================================
// Global Descriptor Table (GDT) + Task State Segment (TSS)
//
// Warum wir das brauchen, obwohl x86_64 "flat memory" nutzt:
// - CS/SS müssen trotzdem auf gültige Deskriptoren zeigen
// - Für Ring0<->Ring3-Wechsel (Syscalls, User-Prozesse) braucht die
//   CPU eine TSS, um zu wissen, welcher Kernel-Stack beim Wechsel
//   von Ring3 nach Ring0 zu benutzen ist (RSP0-Feld der TSS)
// ============================================================

// Segment-Selektoren (Byte-Offsets in die GDT).
// Reihenfolge ist bewusst so gewählt, wie es die spätere
// SYSCALL/SYSRET-Instruktion später erwartet (STAR-MSR-Layout).
enum GdtSelector : uint16_t {
    GDT_NULL        = 0x00,
    GDT_KERNEL_CODE = 0x08,
    GDT_KERNEL_DATA = 0x10,
    GDT_USER_DATA   = 0x18,
    GDT_USER_CODE   = 0x20,
    GDT_TSS         = 0x28,   // belegt 2 Einträge (TSS-Deskriptor ist 16 Byte im Long Mode!)
};

// Klassischer 8-Byte GDT-Eintrag (für Code-/Datensegmente)
struct __attribute__((packed)) GdtEntry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
};

// TSS-Deskriptor ist im Long Mode 16 Byte (erweitert um base_upper + reserved)
struct __attribute__((packed)) TssDescriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
};

struct __attribute__((packed)) GdtPointer {
    uint16_t limit;
    uint64_t base;
};

// Task State Segment (x86_64-Variante).
// Im Long Mode NICHT mehr für Task-Switching genutzt (das machen wir ja
// selbst via context_switch!) - relevant ist hier NUR rsp0: der Stack,
// den die CPU automatisch lädt, wenn ein Interrupt/Syscall aus Ring3
// in Ring0 wechselt.
struct __attribute__((packed)) Tss {
    uint32_t reserved0;
    uint64_t rsp0;      // Kernel-Stack für Ring3->Ring0 Wechsel
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];    // Interrupt Stack Table (z.B. für Double-Fault-Handler)
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
};

extern "C" {

// Baut GDT + TSS auf und lädt sie via lgdt + ltr.
// Muss einmalig früh in kernel_main() aufgerufen werden.
void gdt_init();

// Aktualisiert den Kernel-Stack, den die CPU bei Ring3->Ring0-Wechseln
// benutzt. Muss bei JEDEM Task-Switch neu gesetzt werden, falls du
// später User-Mode-Tasks hast (jeder Task hat seinen eigenen Kernel-Stack).
void tss_set_kernel_stack(uintptr_t rsp0);

}
