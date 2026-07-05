#pragma once
#include <stdint.h>

// ============================================================
// 8259 PIC (Programmable Interrupt Controller)
//
// Standardmäßig sind IRQ0-7 auf die Interrupt-Vektoren 0x08-0x0F
// gemappt - das kollidiert mit den CPU-Exceptions (0x00-0x1F)!
// Deshalb müssen wir den PIC "remappen", bevor wir Interrupts
// aktivieren, sonst ist ein Timer-Interrupt (IRQ0) nicht von einer
// Divide-by-Zero-Exception (Vektor 0) zu unterscheiden.
// ============================================================

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

// Nach dem Remap: IRQ0-7 -> Vektoren 0x20-0x27, IRQ8-15 -> 0x28-0x2F
#define PIC_IRQ_OFFSET_1 0x20
#define PIC_IRQ_OFFSET_2 0x28

extern "C" {

void pic_remap();

// Muss am ENDE jedes Hardware-Interrupt-Handlers aufgerufen werden,
// sonst sendet der PIC nie wieder Interrupts derselben (oder niedrigerer)
// Priorität!
void pic_send_eoi(uint8_t irq);

// Einzelne IRQ maskieren/demaskieren (z.B. Tastatur erst aktivieren,
// wenn der Treiber dafür bereit ist)
void pic_set_mask(uint8_t irq);
void pic_clear_mask(uint8_t irq);

}
