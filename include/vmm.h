#pragma once
#include <stdint.h>

// ============================================================
// Virtual Memory Manager (VMM)
// x86_64 4-Level Paging: PML4 -> PDPT -> PD -> PT
// Jede Tabelle: 512 Einträge x 8 Byte = 4096 Byte (= 1 Page)
// ============================================================

// Flags für Page Table Entries (PTE)
#define PTE_PRESENT     (1ULL << 0)  // Seite ist im Speicher vorhanden
#define PTE_WRITABLE    (1ULL << 1)  // Schreibzugriff erlaubt
#define PTE_USER        (1ULL << 2)  // Zugriff aus Ring 3 (User-Space) erlaubt
#define PTE_WRITETHROUGH (1ULL << 3)
#define PTE_NOCACHE     (1ULL << 4)
#define PTE_HUGE        (1ULL << 7)  // 2MiB/1GiB-Seite statt 4KiB
#define PTE_NX          (1ULL << 63) // No-Execute (Daten dürfen nicht ausgeführt werden)

// Maskiert die physische Adresse aus einem Entry (Bits 12-51)
#define PTE_ADDR_MASK   0x000FFFFFFFFFF000ULL

// Ein einzelner 64-bit Page-Table-Eintrag
using page_table_entry_t = uint64_t;

// Eine Page-Table-Ebene = 512 Einträge = exakt 4096 Byte
struct alignas(4096) page_table_t {
    page_table_entry_t entries[512];
};

extern "C" {

    // Legt eine frische, leere PML4-Root-Tabelle an (für neuen Adressraum,
    // z.B. beim Erstellen eines neuen Prozesses) und gibt ihre physische
    // Adresse zurück.
    uintptr_t vmm_create_address_space();

    // Mappt eine virtuelle 4KiB-Seite auf eine physische 4KiB-Seite
    // innerhalb des durch pml4_phys beschriebenen Adressraums.
    // Legt fehlende Zwischenebenen (PDPT/PD/PT) bei Bedarf automatisch an.
    void vmm_map_page(uintptr_t pml4_phys, uintptr_t virt_addr,
        uintptr_t phys_addr, uint64_t flags);

    // Entfernt ein Mapping (macht die Page invalide, TLB wird invalidiert)
    void vmm_unmap_page(uintptr_t pml4_phys, uintptr_t virt_addr);

    // Lädt einen Adressraum in CR3 -> aktiviert ihn für die CPU
    void vmm_switch_address_space(uintptr_t pml4_phys);

    // Löst eine virtuelle Adresse in ihre physische Adresse auf (Debug/Fault-Handler)
    uintptr_t vmm_translate(uintptr_t pml4_phys, uintptr_t virt_addr);

} // extern "C"
