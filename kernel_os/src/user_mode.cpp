#include "user_mode.h"
#include "gdt.h"
#include "pmm.h"
#include "task.h"
#include "vmm.h"

namespace {
    // Liegt außerhalb des beim Booten angelegten 1-GiB-Identity-Mappings.
    // Dadurch kollidiert die feingranulare User-Abbildung nicht mit Huge-Pages.
    constexpr uintptr_t USER_CODE_ADDRESS = 0x0000004000000000ULL;
    constexpr uintptr_t USER_STACK_ADDRESS = USER_CODE_ADDRESS + PAGE_SIZE;

    uintptr_t demo_entry_address = 0;
    uintptr_t demo_stack_top = 0;

    extern "C" void user_program_start();
    extern "C" void enter_user_mode(uintptr_t entry_address, uintptr_t stack_top);

    uintptr_t current_address_space() {
        uintptr_t pml4_phys;
        asm volatile("mov %%cr3, %0" : "=r"(pml4_phys));
        return pml4_phys;
    }
}

extern "C" bool user_mode_prepare_demo_task(Task* task) {
    if (!task) {
        return false;
    }

    uintptr_t pml4_phys = vmm_create_address_space();
    uintptr_t user_stack_phys = pmm_alloc_page();
    if (pml4_phys == 0 || user_stack_phys == 0) {
        return false;
    }

    // Keep the kernel identity map supervisor-only. The separate user mapping
    // below lives in PDPT slot 1 (at 1 GiB), so it does not expose kernel pages.
    page_table_t* user_pml4 = reinterpret_cast<page_table_t*>(pml4_phys);
    page_table_t* kernel_pml4 = reinterpret_cast<page_table_t*>(current_address_space());
    user_pml4->entries[0] = kernel_pml4->entries[0];

    uintptr_t program_phys = reinterpret_cast<uintptr_t>(user_program_start);
    uintptr_t program_page = program_phys & ~(PAGE_SIZE - 1);
    uintptr_t program_offset = program_phys & (PAGE_SIZE - 1);

    vmm_map_page(pml4_phys, USER_CODE_ADDRESS, program_page, PTE_USER);
    vmm_map_page(pml4_phys, USER_STACK_ADDRESS, user_stack_phys, PTE_USER | PTE_WRITABLE | PTE_NX);

    task->pml4_phys = pml4_phys;
    demo_entry_address = USER_CODE_ADDRESS + program_offset;
    demo_stack_top = USER_STACK_ADDRESS + PAGE_SIZE;
    return true;
}

extern "C" void user_mode_demo_thread() {
    enter_user_mode(demo_entry_address, demo_stack_top);

    // enter_user_mode() returns only if iretq failed unexpectedly.
    for (;;) {
        asm volatile("hlt");
    }
}
