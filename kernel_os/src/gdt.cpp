#include "gdt.h"

namespace {
    // 5 normale Deskriptoren + 1 TSS-Deskriptor (der 2 Slots belegt) = 7 Slots
    GdtEntry gdt[7];
    GdtPointer gdt_ptr;
    Tss tss;

    void set_gdt_entry(int index, uint32_t base, uint32_t limit,
                        uint8_t access, uint8_t granularity) {
        gdt[index].limit_low    = limit & 0xFFFF;
        gdt[index].base_low     = base & 0xFFFF;
        gdt[index].base_middle  = (base >> 16) & 0xFF;
        gdt[index].access       = access;
        gdt[index].granularity  = ((limit >> 16) & 0x0F) | (granularity & 0xF0);
        gdt[index].base_high    = (base >> 24) & 0xFF;
    }

    void set_tss_entry(uint64_t base, uint32_t limit) {
        // TSS-Deskriptor braucht 16 Byte -> wir "casten" zwei GdtEntry-Slots
        // in einen TssDescriptor. Ab GDT_TSS/8 im Array.
        TssDescriptor* desc = reinterpret_cast<TssDescriptor*>(&gdt[GDT_TSS / 8]);

        desc->limit_low   = limit & 0xFFFF;
        desc->base_low    = base & 0xFFFF;
        desc->base_middle = (base >> 16) & 0xFF;
        desc->access      = 0x89; // Present, Ring0, Type=0x9 (64-bit TSS, available)
        desc->granularity = (limit >> 16) & 0x0F;
        desc->base_high   = (base >> 24) & 0xFF;
        desc->base_upper  = (base >> 32) & 0xFFFFFFFF;
        desc->reserved    = 0;
    }
}

// In assembly implementiert (siehe gdt_flush.S) - lädt GDTR und die
// Segment-Register neu. Das muss in Assembly passieren, weil man CS
// nicht direkt per mov setzen kann (nur über far jump/ret).
extern "C" void gdt_flush(uint64_t gdt_ptr_addr);
extern "C" void tss_flush();

extern "C" void gdt_init() {
    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base  = reinterpret_cast<uint64_t>(&gdt);

    // --- Null-Deskriptor (von der CPU vorgeschrieben) ---
    set_gdt_entry(0, 0, 0, 0, 0);

    // --- Kernel Code Segment (Ring 0) ---
    // Access: Present(1) DPL=00 S=1 Type=Code,Execute/Read(1010)
    // Granularity: Long-Mode-Bit (L=1) gesetzt statt Size(D/B)
    set_gdt_entry(1, 0, 0, 0x9A, 0x20);

    // --- Kernel Data Segment (Ring 0) ---
    set_gdt_entry(2, 0, 0, 0x92, 0x00);

    // --- User Data Segment (Ring 3) --- DPL=11
    set_gdt_entry(3, 0, 0, 0xF2, 0x00);

    // --- User Code Segment (Ring 3) ---
    set_gdt_entry(4, 0, 0, 0xFA, 0x20);

    // --- TSS ---
    // TSS nullen und Basis-Werte setzen
    for (uint64_t i = 0; i < sizeof(Tss); i++) {
        reinterpret_cast<uint8_t*>(&tss)[i] = 0;
    }
    tss.iomap_base = sizeof(Tss); // kein I/O-Bitmap -> auf Ende der Struktur zeigen

    set_tss_entry(reinterpret_cast<uint64_t>(&tss), sizeof(Tss) - 1);

    gdt_flush(reinterpret_cast<uint64_t>(&gdt_ptr));
    tss_flush();
}

extern "C" void tss_set_kernel_stack(uintptr_t rsp0) {
    tss.rsp0 = rsp0;
}
