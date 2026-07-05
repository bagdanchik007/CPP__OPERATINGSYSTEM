#pragma once
#include <stdint.h>

// ============================================================
// PIT (Programmable Interval Timer, Intel 8253/8254)
// Erzeugt den periodischen Interrupt auf IRQ0, den wir für
// präemptives Scheduling nutzen.
// ============================================================

extern "C" {

// Konfiguriert den PIT, damit er mit der angegebenen Frequenz (Hz)
// IRQ0 auslöst. Übliche Werte: 100-1000 Hz.
void pit_init(uint32_t frequency_hz);

}
