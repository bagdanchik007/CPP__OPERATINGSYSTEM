#pragma once
#include <stdint.h>

// ============================================================
// PS/2 keyboard (first controller port, scan-code set 1)
//
// The interrupt handler stores translated ASCII characters in a
// ring buffer so code outside the IRQ context does not have to
// interpret scan codes.
// ============================================================

extern "C" {

void keyboard_init();
void keyboard_handle_irq();

bool keyboard_char_available();
char keyboard_read_char();

}
