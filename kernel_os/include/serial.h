#pragma once
#include <stdint.h>

// ============================================================
// 16550 UART (COM1)
// ============================================================

extern "C" {

void serial_init();
void serial_write_char(char character);
void serial_write(const char* string);

}
