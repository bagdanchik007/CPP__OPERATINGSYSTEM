#pragma once
#include <stdint.h>

// ============================================================
// Kernel console
//
// Writes ordinary output to both the VGA text buffer and COM1.
// printk() supports %s, %c, %d, %u, %x and %%.
// ============================================================

extern "C" {

void console_init();
void console_put_char(char character);
void console_write(const char* string);
void console_put_at(int row, int column, char character, uint8_t color);
void printk(const char* format, ...);

}
