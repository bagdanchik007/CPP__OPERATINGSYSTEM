#pragma once
#include "idt.h"

extern "C" {

// Wird von isr_common_stub (isr_stubs.S) für CPU-Exceptions (0-31) aufgerufen.
void exception_handler(InterruptFrame* frame);

// Wird von irq_common_stub (isr_stubs.S) für Hardware-Interrupts aufgerufen.
void irq_handler(InterruptFrame* frame);

}
