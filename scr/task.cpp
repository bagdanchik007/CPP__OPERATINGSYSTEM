#include "task.h"
#include "pmm.h"


namespace {
    Task task_pool[MAX_TASKS];
    bool task_slot_used[MAX_TASKS] = {};
    pid_t next_pid = 1;

    // Größe des Kernel-Stacks pro Task: 4 Seiten = 16 KiB
    constexpr uint64_t KERNEL_STACK_PAGES = 4;

    Task* allocate_task_slot() {
        for (int i = 0; i < MAX_TASKS; i++) {
            if (!task_slot_used[i]) {
                task_slot_used[i] = true;
                return &task_pool[i];
            }
        }
        return nullptr; // Kein freier Slot mehr -> in echtem OS: Fehler behandeln
    }

    void copy_name(char* dst, const char* src, uint64_t max_len) {
        uint64_t i = 0;
        while (src[i] != '\0' && i < max_len - 1) {
            dst[i] = src[i];
            i++;
        }
        dst[i] = '\0';
    }
}

extern "C" Task* task_create_kernel_thread(void (*entry_point)(), const char* name) {
    Task* task = allocate_task_slot();
    if (!task) return nullptr;

    // --- Kernel-Stack allozieren ---
    // Wir holen mehrere physische Seiten. Da wir (siehe vmm.cpp) im Kernel
    // von einem identity-mapped physischen Speicher ausgehen, können wir
    // die physische Adresse direkt als nutzbaren Stack-Pointer verwenden.
    uintptr_t stack_base = pmm_alloc_page();
    for (uint64_t i = 1; i < KERNEL_STACK_PAGES; i++) {
        pmm_alloc_page(); // Für dieses einfache Beispiel gehen wir von
        // physisch aufeinanderfolgenden Seiten aus.
        // Produktionsreif: einzeln mappen statt annehmen!
    }
    uintptr_t stack_top = stack_base + (KERNEL_STACK_PAGES * PAGE_SIZE);

    // --- Stack für den ERSTEN context_switch() vorbereiten ---
    // context_switch() (siehe context_switch.S) macht am Ende ein `ret`.
    // Damit dieses `ret` beim allerersten Wechsel in entry_point springt,
    // legen wir manuell einen "gefälschten" CpuContext auf den Stack, dessen
    // rip-Feld auf entry_point zeigt. Die restlichen Register sind für den
    // Start irrelevant und werden mit 0 initialisiert.
    uintptr_t sp = stack_top;
    sp -= sizeof(CpuContext);
    CpuContext* ctx = reinterpret_cast<CpuContext*>(sp);
    ctx->r15 = 0;
    ctx->r14 = 0;
    ctx->r13 = 0;
    ctx->r12 = 0;
    ctx->rbx = 0;
    ctx->rbp = 0;
    ctx->rip = reinterpret_cast<uint64_t>(entry_point);

    task->pid = next_pid++;
    task->state = TaskState::READY;
    task->kernel_stack_base = stack_base;
    task->kernel_stack_top = stack_top;
    task->saved_rsp = sp;      // context_switch() wird rsp hierauf setzen
    task->pml4_phys = 0;       // 0 = "nutzt den Kernel-Adressraum" (kein eigener)
    task->next = nullptr;

    copy_name(task->name, name, sizeof(task->name));

    return task;
}
