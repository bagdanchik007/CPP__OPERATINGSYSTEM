#include "serial.h"
#include <stddef.h>

namespace {
    constexpr uint16_t COM1_BASE = 0x3F8;
    constexpr uint16_t COM1_DATA = COM1_BASE;
    constexpr uint16_t COM1_INTERRUPT_ENABLE = COM1_BASE + 1;
    constexpr uint16_t COM1_FIFO_CONTROL = COM1_BASE + 2;
    constexpr uint16_t COM1_LINE_CONTROL = COM1_BASE + 3;
    constexpr uint16_t COM1_MODEM_CONTROL = COM1_BASE + 4;
    constexpr uint16_t COM1_LINE_STATUS = COM1_BASE + 5;

    inline void outb(uint16_t port, uint8_t value) {
        asm volatile("outb %0, %1" :: "a"(value), "Nd"(port));
    }

    inline uint8_t inb(uint16_t port) {
        uint8_t value;
        asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
        return value;
    }

    bool transmitter_empty() {
        return (inb(COM1_LINE_STATUS) & 0x20) != 0;
    }
}

extern "C" void serial_init() {
    outb(COM1_INTERRUPT_ENABLE, 0x00); // Disable UART interrupts.
    outb(COM1_LINE_CONTROL, 0x80);     // Enable divisor latch access.
    outb(COM1_DATA, 0x03);             // 38400 baud divisor, low byte.
    outb(COM1_INTERRUPT_ENABLE, 0x00); // 38400 baud divisor, high byte.
    outb(COM1_LINE_CONTROL, 0x03);     // 8 data bits, no parity, one stop bit.
    outb(COM1_FIFO_CONTROL, 0xC7);     // Enable and clear FIFO, 14-byte threshold.
    outb(COM1_MODEM_CONTROL, 0x0B);    // IRQs enabled, RTS/DSR set.
}

extern "C" void serial_write_char(char character) {
    while (!transmitter_empty()) {
    }
    outb(COM1_DATA, static_cast<uint8_t>(character));
}

extern "C" void serial_write(const char* string) {
    for (size_t index = 0; string[index] != '\0'; index++) {
        if (string[index] == '\n') {
            serial_write_char('\r');
        }
        serial_write_char(string[index]);
    }
}
