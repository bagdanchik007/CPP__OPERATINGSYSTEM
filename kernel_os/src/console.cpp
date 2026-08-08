#include "console.h"
#include "serial.h"
#include <stddef.h>
#include <stdarg.h>

namespace {
    constexpr int VGA_WIDTH = 80;
    constexpr int VGA_HEIGHT = 25;
    constexpr uint8_t DEFAULT_COLOR = 0x0F;

    volatile uint16_t* vga_buffer = reinterpret_cast<volatile uint16_t*>(0xB8000);
    int cursor_row = 0;
    int cursor_column = 0;

    void scroll_if_needed() {
        if (cursor_row < VGA_HEIGHT) {
            return;
        }

        for (int row = 1; row < VGA_HEIGHT; row++) {
            for (int column = 0; column < VGA_WIDTH; column++) {
                vga_buffer[(row - 1) * VGA_WIDTH + column] =
                    vga_buffer[row * VGA_WIDTH + column];
            }
        }

        for (int column = 0; column < VGA_WIDTH; column++) {
            vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + column] =
                (DEFAULT_COLOR << 8) | ' ';
        }
        cursor_row = VGA_HEIGHT - 1;
    }

    void write_unsigned(uint64_t value, uint32_t base) {
        char digits[16];
        uint32_t count = 0;

        do {
            uint64_t digit = value % base;
            digits[count++] = (digit < 10) ? static_cast<char>('0' + digit)
                                           : static_cast<char>('a' + digit - 10);
            value /= base;
        } while (value != 0);

        while (count > 0) {
            console_put_char(digits[--count]);
        }
    }
}

extern "C" void console_init() {
    serial_init();
    cursor_row = 0;
    cursor_column = 0;

    for (int row = 0; row < VGA_HEIGHT; row++) {
        for (int column = 0; column < VGA_WIDTH; column++) {
            vga_buffer[row * VGA_WIDTH + column] = (DEFAULT_COLOR << 8) | ' ';
        }
    }
}

extern "C" void console_put_char(char character) {
    if (character == '\n') {
        serial_write_char('\r');
        serial_write_char('\n');
        cursor_column = 0;
        cursor_row++;
    } else if (character == '\r') {
        cursor_column = 0;
    } else if (character == '\b') {
        if (cursor_column > 0) {
            cursor_column--;
            vga_buffer[cursor_row * VGA_WIDTH + cursor_column] =
                (DEFAULT_COLOR << 8) | ' ';
        }
        serial_write_char('\b');
    } else {
        vga_buffer[cursor_row * VGA_WIDTH + cursor_column] =
            (DEFAULT_COLOR << 8) | static_cast<uint8_t>(character);
        serial_write_char(character);
        cursor_column++;
        if (cursor_column == VGA_WIDTH) {
            cursor_column = 0;
            cursor_row++;
        }
    }

    scroll_if_needed();
}

extern "C" void console_write(const char* string) {
    for (size_t index = 0; string[index] != '\0'; index++) {
        console_put_char(string[index]);
    }
}

extern "C" void console_put_at(int row, int column, char character, uint8_t color) {
    if (row < 0 || row >= VGA_HEIGHT || column < 0 || column >= VGA_WIDTH) {
        return;
    }
    vga_buffer[row * VGA_WIDTH + column] = (color << 8) | static_cast<uint8_t>(character);
}

extern "C" void printk(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);

    for (size_t index = 0; format[index] != '\0'; index++) {
        if (format[index] != '%') {
            console_put_char(format[index]);
            continue;
        }

        index++;
        switch (format[index]) {
            case '%': console_put_char('%'); break;
            case 'c': console_put_char(static_cast<char>(va_arg(arguments, int))); break;
            case 's': {
                const char* string = va_arg(arguments, const char*);
                console_write(string ? string : "(null)");
                break;
            }
            case 'd': {
                int value = va_arg(arguments, int);
                if (value < 0) {
                    console_put_char('-');
                    write_unsigned(static_cast<uint32_t>(-(value + 1)) + 1, 10);
                } else {
                    write_unsigned(static_cast<uint32_t>(value), 10);
                }
                break;
            }
            case 'u': write_unsigned(va_arg(arguments, unsigned int), 10); break;
            case 'x': write_unsigned(va_arg(arguments, unsigned int), 16); break;
            default:
                console_put_char('%');
                console_put_char(format[index]);
                break;
        }
    }

    va_end(arguments);
}
