#pragma once

struct Task;

// ============================================================
// Ring-3 demo task
// ============================================================

extern "C" {

// Creates an address space with a small ring-3 program and stack and
// attaches it to the supplied kernel task.
bool user_mode_prepare_demo_task(Task* task);

// Kernel-thread entry point. This never returns after iretq enters ring 3.
void user_mode_demo_thread();

}
