#include "pmm.h"
#include "task.h"
#include "scheduler.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "pit.h"
#include <stdint.h>

// ------------------------------------------------------------
// Sehr einfache VGA-Textmodus-Ausgabe, nur um sichtbar zu machen,
// dass der Kernel wirklich läuft. Später durch einen richtigen
// Framebuffer-Treiber ersetzen.
// ------------------------------------------------------------
namespace {
    volatile uint16_t* vga_buffer = reinterpret_cast<volatile uint16_t*>(0xB8000);

    void vga_print(const char* str, uint8_t color = 0x0F) {
        static int cursor = 0;
        for (int i = 0; str[i] != '\0'; i++) {
            vga_buffer[cursor++] = (color << 8) | str[i];
        }
    }
}

// Wird von boot.S mit (Multiboot-Magic, Multiboot-Info-Pointer) aufgerufen
// -> System V AMD64 ABI: 1. Arg = rdi, 2. Arg = rsi
extern "C" void kernel_main(uint32_t multiboot_magic, uintptr_t multiboot_info_addr) {
    vga_print("Kernel gestartet - Long Mode aktiv!");

    // --- Reihenfolge ist wichtig! ---
    // 1. GDT zuerst: legt gültige Kernel-Segmente + TSS fest
    // 2. IDT danach: braucht den Kernel-Code-Selektor aus der GDT
    // 3. PIC remappen, BEVOR Interrupts aktiviert werden (sonst
    //    kollidieren Hardware-IRQs mit CPU-Exceptions!)
    // 4. PIT konfigurieren: erzeugt den periodischen Timer-Interrupt
    // 5. Erst ganz am Schluss "sti" -> Interrupts scharf schalten
    gdt_init();
    idt_init();
    pic_remap();
    pit_init(100); // 100 Hz -> alle 10ms ein Scheduler-Tick

    vga_print(" | GDT+IDT+PIC+PIT initialisiert.");

    // TODO: multiboot_info_addr auswerten, um die ECHTE physische

    // Memory Map vom Bootloader zu bekommen, statt hartkodierter Werte.
    // Für den allerersten Test reicht eine angenommene Größe:
    constexpr uint64_t ASSUMED_RAM_PAGES = 32768; // 128 MiB / 4KiB

    // Bitmap direkt hinter dem Kernel-Image platzieren (grob geschätzt,
    // in der Praxis: Symbol aus dem Linker-Script nehmen, z.B. `_kernel_end`)
    constexpr uintptr_t BITMAP_ADDR = 0x200000; // 2 MiB

    pmm_init(BITMAP_ADDR, ASSUMED_RAM_PAGES);

    // Die ersten paar MB (Kernel-Image + Bitmap-Bereich) als reserviert markieren
    pmm_mark_reserved(0x100000, 0x200000); // 1MiB - 3MiB grob abdecken

    scheduler_init();

    // Zwei Test-Threads erstellen (siehe unten) -> klassischer
    // "ping/pong"-Test um zu sehen, ob der Scheduler wirklich umschaltet.
    extern void thread_a_entry();
    extern void thread_b_entry();

    Task* task_a = task_create_kernel_thread(thread_a_entry, "thread_a");
    Task* task_b = task_create_kernel_thread(thread_b_entry, "thread_b");

    scheduler_add_task(task_a);
    scheduler_add_task(task_b);

    // Ersten Task manuell anschieben (current_task ist ja noch NULL) -
    // ab jetzt übernimmt der Timer-Interrupt automatisch alle weiteren Switches.
    scheduler_tick();

    // Interrupts scharf schalten -> ab hier läuft der Timer und
    // unterbricht/schaltet automatisch zwischen Tasks um (Preemption!).
    asm volatile("sti");

    // Kernel-Idle-Loop: CPU anhalten, bis der nächste Interrupt kommt
    // (spart Strom, verglichen mit einer busy-loop).
    while (true) {
        asm volatile("hlt");
    }
}

// Platzhalter-Test-Threads, bis du echte Tasks hast
extern "C" void thread_a_entry() {
    while (true) { /* würde z.B. "A" ausgeben */ }
}
extern "C" void thread_b_entry() {
    while (true) { /* würde z.B. "B" ausgeben */ }
}
