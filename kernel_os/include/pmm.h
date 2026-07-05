#pragma once
#include <stdint.h>
#include <stddef.h>

// ============================================================
// Physical Memory Manager (PMM)
// Bitmap-basierter Page Frame Allocator
// 1 Bit pro physischer 4KiB-Seite: 0 = frei, 1 = belegt
// ============================================================

#define PAGE_SIZE 4096ULL

extern "C" {

// Initialisiert den Allocator.
// bitmap_addr: physische Adresse, an der die Bitmap im Speicher liegt
//              (muss vom Bootloader/Kernel reserviert sein!)
// total_pages: Gesamtzahl der physischen 4KiB-Seiten im System
void pmm_init(uintptr_t bitmap_addr, uint64_t total_pages);

// Markiert einen Bereich physischer Seiten als reserviert
// (z.B. Kernel-Image, Bootloader-Strukturen, MMIO-Bereiche)
void pmm_mark_reserved(uintptr_t base_addr, uint64_t length_bytes);

// Reserviert eine freie physische Seite und gibt ihre physische
// Adresse zurück. Gibt 0 zurück, wenn kein Speicher mehr frei ist.
uintptr_t pmm_alloc_page();

// Gibt eine zuvor allozierte physische Seite wieder frei.
void pmm_free_page(uintptr_t phys_addr);

// Debug/Statistik
uint64_t pmm_get_free_page_count();
uint64_t pmm_get_total_page_count();

} // extern "C"
