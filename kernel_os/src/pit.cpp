#include "pit.h"

namespace {
    inline void outb(uint16_t port, uint8_t val) {
        asm volatile("outb %0, %1" :: "a"(val), "Nd"(port));
    }

    constexpr uint32_t PIT_BASE_FREQUENCY = 1193182; // Hz, feste Hardware-Taktrate
    constexpr uint16_t PIT_CHANNEL0_DATA  = 0x40;
    constexpr uint16_t PIT_COMMAND        = 0x43;
}

extern "C" void pit_init(uint32_t frequency_hz) {
    uint32_t divisor = PIT_BASE_FREQUENCY / frequency_hz;

    // Command Byte: Channel 0, Access Mode lo/hi, Mode 3 (Square Wave), Binary
    outb(PIT_COMMAND, 0x36);

    // Divisor als zwei Bytes senden (erst low, dann high)
    outb(PIT_CHANNEL0_DATA, divisor & 0xFF);
    outb(PIT_CHANNEL0_DATA, (divisor >> 8) & 0xFF);
}
