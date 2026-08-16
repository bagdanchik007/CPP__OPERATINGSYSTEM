#include "scheduler.h"
#include "gdt.h"
#include "vmm.h"

// ------------------------------------------------------------
// Ready-Queue als einfache intrusive Ringliste (via Task::next).
// "Intrusiv" heißt: der next-Zeiger liegt direkt in der Task-Struktur
// selbst -> keine zusätzliche Allokation für die Queue nötig.
// ------------------------------------------------------------
namespace {
    Task* current_task = nullptr; // Task, der GERADE auf der CPU läuft
    Task* ready_head    = nullptr; // Kopf der Ready-Queue
    Task* ready_tail    = nullptr; // Ende der Ready-Queue (für O(1) Einfügen)
    uintptr_t kernel_pml4_phys = 0;
}

extern "C" void scheduler_init() {
    current_task = nullptr;
    ready_head   = nullptr;
    ready_tail   = nullptr;
    asm volatile("mov %%cr3, %0" : "=r"(kernel_pml4_phys));
}

extern "C" void scheduler_add_task(Task* task) {
    if (!task) return;
    task->state = TaskState::READY;
    task->next  = nullptr;

    if (!ready_head) {
        // Queue war leer
        ready_head = task;
        ready_tail = task;
    } else {
        ready_tail->next = task;
        ready_tail = task;
    }
}

extern "C" void scheduler_start() {
    if (current_task || !ready_head) {
        return;
    }

    Task* next_task = ready_head;
    ready_head = ready_head->next;
    if (!ready_head) ready_tail = nullptr;

    next_task->next = nullptr;
    next_task->state = TaskState::RUNNING;
    current_task = next_task;
    tss_set_kernel_stack(next_task->kernel_stack_top);
    if (next_task->pml4_phys != 0) {
        vmm_switch_address_space(next_task->pml4_phys);
    }
    context_switch(nullptr, next_task);
}

extern "C" Task* scheduler_current_task() {
    return current_task;
}

extern "C" void task_yield() {
    // Kooperativer Wechsel ist technisch dasselbe wie ein Timer-Tick -
    // der Scheduler unterscheidet nicht, WARUM gewechselt wird.
    scheduler_tick();
}

extern "C" void scheduler_tick() {
    // Kein Task läuft noch nicht mal -> ersten Task aus der Queue starten.
    if (!current_task) {
        if (!ready_head) return; // gar nichts zu tun
        current_task = ready_head;
        ready_head = ready_head->next;
        if (!ready_head) ready_tail = nullptr;
        current_task->state = TaskState::RUNNING;
        tss_set_kernel_stack(current_task->kernel_stack_top);
        return; // Hinweis: Der allererste "Einstieg" läuft normalerweise
                // separat über einen expliziten context_switch(nullptr, task).
    }

    // Kein anderer Task wartet -> aktueller Task läuft einfach weiter.
    if (!ready_head) return;

    // --- Round-Robin: aktuellen Task ans Ende der Queue anhängen ---
    Task* prev_task = current_task;
    if (prev_task->state == TaskState::RUNNING) {
        prev_task->state = TaskState::READY;
        prev_task->next = nullptr;
        if (!ready_head) { ready_head = prev_task; ready_tail = prev_task; }
        else { ready_tail->next = prev_task; ready_tail = prev_task; }
    }

    // --- Nächsten Task aus der Queue holen ---
    Task* next_task = ready_head;
    ready_head = ready_head->next;
    if (!ready_head) ready_tail = nullptr;

    next_task->state = TaskState::RUNNING;
    current_task = next_task;
    tss_set_kernel_stack(next_task->kernel_stack_top);

    // Adressraum wechseln, FALLS der neue Task einen eigenen hat
    // (0 = teilt sich den Kernel-Adressraum, kein Wechsel nötig).
    vmm_switch_address_space(next_task->pml4_phys != 0 ?
                             next_task->pml4_phys : kernel_pml4_phys);

    // Der eigentliche Register-Wechsel passiert hier in Assembly.
    context_switch(prev_task, next_task);

    // WICHTIG: Wenn dieser Task später wieder drangenommen wird, kehrt
    // die Ausführung genau HIER zurück (context_switch kehrt über `ret`
    // zurück, dessen Zieladresse beim letzten Switch gesichert wurde).
}
