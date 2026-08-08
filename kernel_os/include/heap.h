#pragma once
#include <stddef.h>

// ============================================================
// Simple kernel heap
//
// A static BSS-backed heap with first-fit allocation and free-block
// coalescing. The allocator is intended for kernel context only and
// must not be called from an interrupt handler.
// ============================================================

extern "C" {

void heap_init();
void* kmalloc(size_t size);
void kfree(void* pointer);

}
