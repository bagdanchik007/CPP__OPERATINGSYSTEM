#include "pmm.h"

// ------------------------------------------------------------
// Interner Zustand des Allocators
// ------------------------------------------------------------
namespace {
    uint8_t* bitmap        = nullptr; // Zeiger auf die Bitmap (identity-mapped angenommen)
    uint64_t total_pages   = 0;
    uint64_t free_pages    = 0;
    uint64_t search_hint   = 0; // letzte bekannte "wahrscheinlich frei"-Position -> O(1) im Best Case
}

// Kleine Helfer, da wir KEINE Standardbibliothek haben.
static inline void set_bit(uint64_t bit) {
    bitmap[bit / 8] |= (1u << (bit % 8));
}
static inline void clear_bit(uint64_t bit) {
    bitmap[bit / 8] &= ~(1u << (bit % 8));
}
static inline bool test_bit(uint64_t bit) {
    return (bitmap[bit / 8] & (1u << (bit % 8))) != 0;
}

extern "C" void pmm_init(uintptr_t bitmap_addr, uint64_t page_count) {
    bitmap      = reinterpret_cast<uint8_t*>(bitmap_addr);
    total_pages = page_count;
    free_pages  = page_count;
    search_hint = 0;

    // Größe der Bitmap in Bytes: 1 Bit pro Seite, aufgerundet
    uint64_t bitmap_bytes = (page_count + 7) / 8;

    // Am Anfang: alles als FREI markieren (Bit = 0)
    for (uint64_t i = 0; i < bitmap_bytes; i++) {
        bitmap[i] = 0x00;
    }

    // Die Bitmap selbst belegt physischen Speicher -> als reserviert markieren,
    // sonst würde sich der Allocator später selbst überschreiben!
    pmm_mark_reserved(bitmap_addr, bitmap_bytes);

    // Physische Adresse 0 ist zugleich der Fehlerwert der Allokations-API und
    // darf deshalb nie an Aufrufer ausgegeben werden.
    pmm_mark_reserved(0, PAGE_SIZE);
}

extern "C" void pmm_mark_reserved(uintptr_t base_addr, uint64_t length_bytes) {
    if (length_bytes == 0 || base_addr >= total_pages * PAGE_SIZE) {
        return;
    }

    uint64_t start_page = base_addr / PAGE_SIZE;
    uint64_t last_byte = length_bytes - 1;
    uint64_t end_page = (last_byte > UINTPTR_MAX - base_addr) ? total_pages :
                        (base_addr + last_byte) / PAGE_SIZE + 1;

    for (uint64_t page = start_page; page < end_page && page < total_pages; page++) {
        if (!test_bit(page)) {
            set_bit(page);
            free_pages--;
        }
    }
}

extern "C" uintptr_t pmm_alloc_page() {
    // Lineare Suche ab dem letzten Hinweis (verhindert, dass wir bei jedem
    // Alloc wieder bei 0 anfangen -> billige Amortisierung).
    for (uint64_t i = 0; i < total_pages; i++) {
        uint64_t page = (search_hint + i) % total_pages;
        if (!test_bit(page)) {
            set_bit(page);
            free_pages--;
            search_hint = page + 1;
            return page * PAGE_SIZE;
        }
    }
    return 0; // Out of memory
}

extern "C" uintptr_t pmm_alloc_pages(uint64_t count) {
    if (count == 0 || count > total_pages) {
        return 0;
    }

    uint64_t run_start = 0;
    uint64_t run_length = 0;
    for (uint64_t page = 0; page < total_pages; page++) {
        if (test_bit(page)) {
            run_length = 0;
            continue;
        }

        if (run_length == 0) {
            run_start = page;
        }
        if (++run_length != count) {
            continue;
        }

        for (uint64_t allocated = run_start; allocated < run_start + count; allocated++) {
            set_bit(allocated);
        }
        free_pages -= count;
        search_hint = run_start + count;
        return run_start * PAGE_SIZE;
    }

    return 0;
}

extern "C" void pmm_free_page(uintptr_t phys_addr) {
    uint64_t page = phys_addr / PAGE_SIZE;
    if (phys_addr % PAGE_SIZE != 0 || page == 0 || page >= total_pages) return;

    if (test_bit(page)) {
        clear_bit(page);
        free_pages++;
    }
}

extern "C" void pmm_free_pages(uintptr_t phys_addr, uint64_t count) {
    if (count == 0 || phys_addr % PAGE_SIZE != 0 || phys_addr == 0) {
        return;
    }

    uint64_t start_page = phys_addr / PAGE_SIZE;
    if (start_page >= total_pages || count > total_pages - start_page) {
        return;
    }

    for (uint64_t page = start_page; page < start_page + count; page++) {
        if (test_bit(page)) {
            clear_bit(page);
            free_pages++;
        }
    }
}

extern "C" uint64_t pmm_get_free_page_count()  { return free_pages; }
extern "C" uint64_t pmm_get_total_page_count() { return total_pages; }
