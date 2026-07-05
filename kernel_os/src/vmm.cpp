#include "vmm.h"
#include "pmm.h"

// ------------------------------------------------------------
// WICHTIGE VEREINFACHUNG für dieses Hobby-OS:
// Wir nehmen an, dass der komplette physische Speicher im Kernel
// 1:1 (identity-mapped) oder über ein festes Offset gemappt ist,
// sodass wir physische Adressen direkt als Zeiger dereferenzieren
// können. In einem echten Higher-Half-Kernel würdest du hier
// stattdessen phys + HHDM_OFFSET rechnen.
// ------------------------------------------------------------
static inline page_table_t* phys_to_virt(uintptr_t phys) {
    return reinterpret_cast<page_table_t*>(phys);
}

// Zerlegt eine virtuelle Adresse in ihre 4 Index-Ebenen + Page-Offset.
// x86_64: Bits [47:39]=PML4, [38:30]=PDPT, [29:21]=PD, [20:12]=PT
struct VAddrIndices {
    uint64_t pml4_i, pdpt_i, pd_i, pt_i;
};
static inline VAddrIndices split_vaddr(uintptr_t v) {
    return {
        (v >> 39) & 0x1FF,
        (v >> 30) & 0x1FF,
        (v >> 21) & 0x1FF,
        (v >> 12) & 0x1FF
    };
}

// Holt (oder erstellt bei Bedarf) die nächsttiefere Tabelle aus einem Entry.
static page_table_t* get_or_create_table(page_table_t* table, uint64_t index, uint64_t flags) {
    page_table_entry_t& entry = table->entries[index];

    if (!(entry & PTE_PRESENT)) {
        // Noch keine Tabelle vorhanden -> neue physische Seite dafür holen
        uintptr_t new_table_phys = pmm_alloc_page();

        // Neue Tabelle nullen (alle Einträge "not present")
        page_table_t* new_table = phys_to_virt(new_table_phys);
        for (int i = 0; i < 512; i++) new_table->entries[i] = 0;

        entry = (new_table_phys & PTE_ADDR_MASK) | PTE_PRESENT | PTE_WRITABLE | flags;
    }

    return phys_to_virt(entry & PTE_ADDR_MASK);
}

extern "C" uintptr_t vmm_create_address_space() {
    uintptr_t pml4_phys = pmm_alloc_page();
    page_table_t* pml4 = phys_to_virt(pml4_phys);
    for (int i = 0; i < 512; i++) pml4->entries[i] = 0;
    return pml4_phys;
}

extern "C" void vmm_map_page(uintptr_t pml4_phys, uintptr_t virt_addr,
                              uintptr_t phys_addr, uint64_t flags) {
    VAddrIndices idx = split_vaddr(virt_addr);

    page_table_t* pml4 = phys_to_virt(pml4_phys);
    page_table_t* pdpt = get_or_create_table(pml4, idx.pml4_i, PTE_USER);
    page_table_t* pd   = get_or_create_table(pdpt, idx.pdpt_i, PTE_USER);
    page_table_t* pt   = get_or_create_table(pd,   idx.pd_i,   PTE_USER);

    // Letzte Ebene: hier liegt die tatsächliche physische Adresse + Flags
    pt->entries[idx.pt_i] = (phys_addr & PTE_ADDR_MASK) | flags | PTE_PRESENT;
}

extern "C" void vmm_unmap_page(uintptr_t pml4_phys, uintptr_t virt_addr) {
    VAddrIndices idx = split_vaddr(virt_addr);
    page_table_t* pml4 = phys_to_virt(pml4_phys);

    page_table_entry_t e1 = pml4->entries[idx.pml4_i];
    if (!(e1 & PTE_PRESENT)) return;
    page_table_t* pdpt = phys_to_virt(e1 & PTE_ADDR_MASK);

    page_table_entry_t e2 = pdpt->entries[idx.pdpt_i];
    if (!(e2 & PTE_PRESENT)) return;
    page_table_t* pd = phys_to_virt(e2 & PTE_ADDR_MASK);

    page_table_entry_t e3 = pd->entries[idx.pd_i];
    if (!(e3 & PTE_PRESENT)) return;
    page_table_t* pt = phys_to_virt(e3 & PTE_ADDR_MASK);

    pt->entries[idx.pt_i] = 0;

    // TLB für diese eine Seite invalidieren, damit die CPU das alte
    // Mapping nicht weiter aus dem Cache verwendet.
    asm volatile("invlpg (%0)" ::"r"(virt_addr) : "memory");
}

extern "C" void vmm_switch_address_space(uintptr_t pml4_phys) {
    // CR3 laden = kompletten Adressraum wechseln (impliziert TLB-Flush,
    // außer bei PCID-Global-Pages, die wir hier nicht nutzen).
    asm volatile("mov %0, %%cr3" ::"r"(pml4_phys) : "memory");
}

extern "C" uintptr_t vmm_translate(uintptr_t pml4_phys, uintptr_t virt_addr) {
    VAddrIndices idx = split_vaddr(virt_addr);
    page_table_t* pml4 = phys_to_virt(pml4_phys);

    page_table_entry_t e1 = pml4->entries[idx.pml4_i];
    if (!(e1 & PTE_PRESENT)) return 0;
    page_table_t* pdpt = phys_to_virt(e1 & PTE_ADDR_MASK);

    page_table_entry_t e2 = pdpt->entries[idx.pdpt_i];
    if (!(e2 & PTE_PRESENT)) return 0;
    page_table_t* pd = phys_to_virt(e2 & PTE_ADDR_MASK);

    page_table_entry_t e3 = pd->entries[idx.pd_i];
    if (!(e3 & PTE_PRESENT)) return 0;
    page_table_t* pt = phys_to_virt(e3 & PTE_ADDR_MASK);

    page_table_entry_t e4 = pt->entries[idx.pt_i];
    if (!(e4 & PTE_PRESENT)) return 0;

    uintptr_t page_base = e4 & PTE_ADDR_MASK;
    uintptr_t offset     = virt_addr & 0xFFF;
    return page_base | offset;
}
