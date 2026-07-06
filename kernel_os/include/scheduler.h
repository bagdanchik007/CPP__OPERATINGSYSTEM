#pragma once
#include "task.h"

// ============================================================
// Round-Robin Scheduler
// ============================================================

extern "C" {

// Muss einmalig beim Booten aufgerufen werden, bevor Tasks laufen.
void scheduler_init();

// Fügt einen Task in die Ready-Queue ein.
void scheduler_add_task(Task* task);

// Wird vom Timer-Interrupt-Handler aufgerufen.
// Wählt den nächsten Task aus der Ready-Queue und stößt den
// eigentlichen Kontextwechsel (context_switch) an.
void scheduler_tick();

// Freiwilliger Wechsel: ein Task ruft das selbst auf, um die CPU
// abzugeben, ohne auf den nächsten Timer-Interrupt zu warten. Nützlich
// zum Testen, bevor Interrupts überhaupt aktiv sind, oder wenn ein Task
// z.B. auf I/O wartet und sinnvoll nichts anderes zu tun hat.
void task_yield();

// Gibt den aktuell laufenden Task zurück.
Task* scheduler_current_task();

// Low-level Kontextwechsel (Assembly, siehe context_switch.S).
// Sichert den Register-Zustand von old_task und lädt den von new_task.
extern void context_switch(Task* old_task, Task* new_task);

}
