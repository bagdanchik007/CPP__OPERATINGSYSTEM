#include "pic.h"

namespace {
    inline void outb(uint16_t port, uint8_t val) {
        asm volatile("outb %0, %1" :: "a"(val), "Nd"(port));
    }
    inline uint8_t inb(uint16_t port) {
        uint8_t ret;
        asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
        return ret;
    }
    inline void io_wait() {
        // Kurze Verzögerung über einen unbenutzten Port - alte PICs
        // brauchen zwischen den Init-Befehlen etwas "Bedenkzeit".
        outb(0x80, 0);
    }
}

extern "C" void pic_remap() {
    uint8_t mask1 = inb(PIC1_DATA); // aktuelle Masken sichern
    uint8_t mask2 = inb(PIC2_DATA);

    // ICW1: Initialisierung starten, Cascade-Modus
    outb(PIC1_COMMAND, 0x11); io_wait();
    outb(PIC2_COMMAND, 0x11); io_wait();

    // ICW2: neue Vektor-Offsets setzen
    outb(PIC1_DATA, PIC_IRQ_OFFSET_1); io_wait();
    outb(PIC2_DATA, PIC_IRQ_OFFSET_2); io_wait();

    // ICW3: Master/Slave-Verkabelung mitteilen
    outb(PIC1_DATA, 0x04); io_wait(); // Slave hängt an IRQ2
    outb(PIC2_DATA, 0x02); io_wait(); // Slave-Identität

    // ICW4: 8086-Modus
    outb(PIC1_DATA, 0x01); io_wait();
    outb(PIC2_DATA, 0x01); io_wait();

    // Ursprüngliche Masken wiederherstellen
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

extern "C" void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, 0x20); // EOI an Slave
    }
    outb(PIC1_COMMAND, 0x20);     // EOI an Master (immer nötig)
}

extern "C" void pic_set_mask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t bit   = (irq < 8) ? irq : irq - 8;
    outb(port, inb(port) | (1 << bit));
}

extern "C" void pic_clear_mask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t bit   = (irq < 8) ? irq : irq - 8;
    outb(port, inb(port) & ~(1 << bit));
}
