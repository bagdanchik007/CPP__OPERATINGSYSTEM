#pragma once
#include <stdint.h>

// ============================================================
// Process Control Block (PCB) / Task-Struktur
// ============================================================

enum class TaskState : uint8_t {
    NEW,
    READY,
    RUNNING,
    BLOCKED,
    TERMINATED
};

// CPU-Register-Kontext für x86_64.
// Layout ist bewusst so gewählt, dass es 1:1 zum Stack-Layout passt,
// das context_switch.S beim Sichern/Wiederherstellen erzeugt (siehe unten).
// Wir sichern hier NUR die callee-saved Register + Instruction Pointer +
// Stack Pointer -> das reicht für einen kooperativen/Timer-Kontextwechsel,
// da der Compiler caller-saved Register bereits vor jedem Funktionsaufruf
// selbst sichert (System V AMD64 ABI).
struct CpuContext {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t rip;   // Rücksprungadresse (wohin nach dem Switch weitergemacht wird)
};

using pid_t = uint32_t;

struct Task {
    pid_t        pid;
    TaskState    state;

    // Kernel-Stack für diesen Task (jeder Task hat seinen eigenen!)
    uintptr_t    kernel_stack_base;   // unterste Adresse (für Freigabe)
    uintptr_t    kernel_stack_top;    // Adresse, mit der der Stack initial startet

    // Gesicherter CPU-Zustand, wenn der Task NICHT läuft.
    // rsp zeigt auf die Stelle im Kernel-Stack, wo CpuContext liegt.
    uint64_t     saved_rsp;

    // Adressraum dieses Tasks (physische Adresse des PML4)
    uintptr_t    pml4_phys;

    // Intrusive Linked List für die Ready-Queue des Schedulers
    Task*        next;

    // Optional: Name fürs Debugging
    char         name[32];
};

extern "C" {

// Initialisiert einen neuen Kernel-Thread:
// - alloziert einen Kernel-Stack
// - bereitet den Stack so vor, dass beim ersten context_switch()
//   direkt in entry_point gesprungen wird
// - setzt State auf READY
Task* task_create_kernel_thread(void (*entry_point)(), const char* name);

}
