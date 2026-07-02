#include "pmm.h"

//Interner Zustand des Allocators

namespace {
	uint8_t* bitmap = nullptr; // Zeiger auf die Bitmap im Speicher
	uint64_t total_pages = 0; // Gesamtzahl der physischen Seiten
	uint64_t free_pages = 0; // Anzahl der freien Seiten
	uint64 search_hint = 0; // Startindex für die Suche nach freien Seiten

}

//Kleine helfer,da wir keine Standartbibliothek haben
static inline void set_bit(uint64_t index) {
	bitmap[index / 8] |= (1 << (index % 8));
}

static inline void clear_bit(uint64_t bit) {
	bitmap[bit / 8] &= ~(1 << (bit % 8));
}
static inline bool test_bit(uint64_t index) {
	return (bitmap[index / 8] & (1 << (index % 8))) != 0;
}

extern "C" void pmm_init(uintptr_t bitmap_addr, uint64_t total_pages_count) {
	bitmap = reinterpret_cast<uint8_t*>(bitmap_addr);
	total_pages = page_count;
	free_pages =  page_count;
	search_hint = 0;
	//Grösse der Bitmap in Bytes berechnen, 1Bit pro Seite, also 1/8 Byte pro Seite
	uint64_t bitmap_bytes = (page_count + 7) / 8; // Aufrunden auf nächstes Byte

	//Am Anfang alle Bits auf 0 setzen, d.h. alle Seiten sind frei
	for (uint64_t i = 0; i < bitmap_bytes; ++i) {
		bitmap[i] = 0x00;
	}
	//Die Bitmap selnst belegt physischer Speicher, also die Seiten, auf denen die Bitmap selbst liegt, als belegt markieren
	//sonst würde sich der Allocator selbst überschreiben
	pmm_mark_reserved(bitmap_addr, bitmap_bytes);
}

extern "C" void pmm_mark_reserved(uintptr_t base_addr, uint64_t length_bytes) {
	//Berechne die Start- und Endseiten
	uint64_t start_page = base_addr / PAGE_SIZE;
	uint64_t end_page = (base_addr + length_bytes + PAGE_SIZE - 1) / PAGE_SIZE; // Aufrunden auf nächste Seite
	for (uint64_t page = start_page; page < end_page; ++page) {
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

extern "C" void pmm_free_page(uintptr_t phys_addr) {
	uint64_t page = phys_addr / PAGE_SIZE;
	if (page >= total_pages) return; // ungültige Adresse, defensiv abfangen

	if (test_bit(page)) {
		clear_bit(page);
		free_pages++;
	}
}

extern "C" uint64_t pmm_get_free_page_count() { return free_pages; }
extern "C" uint64_t pmm_get_total_page_count() { return total_pages; }
