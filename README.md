# Hobby x86_64 Kernel

Ein monolithischer Betriebssystemkern für x86_64, geschrieben in freistehendem C++ und Assembly – von Boot-Prozess bis präemptivem Multitasking.

## Features

- **Multiboot2-kompatibler Boot-Prozess** – Übergang von 32-bit Protected Mode zu 64-bit Long Mode
- **Physical Memory Manager** – Bitmap-basierter Page Frame Allocator
- **Virtual Memory Manager** – x86_64 4-Level Paging (PML4 → PDPT → PD → PT)
- **GDT + TSS** – vollständige Segment-Deskriptoren inkl. Task State Segment für spätere Ring0/Ring3-Wechsel
- **IDT + Exception Handling** – alle 32 CPU-Exceptions abgefangen
- **PIC-Remapping** – Hardware-IRQs sauber von CPU-Exceptions getrennt
- **PIT-Timer** – konfigurierbarer periodischer Interrupt
- **Präemptiver Round-Robin-Scheduler** – automatischer Task-Wechsel per Timer-Interrupt
- **Kontextwechsel in x86_64 Assembly** – vollständige Register-Sicherung/-Wiederherstellung

## Architektur

```
Boot (GRUB, Multiboot2)
   │
   ▼
boot.S — Protected Mode → Long Mode
   │
   ▼
kernel_main()
   │
   ├── gdt_init()      GDT + TSS laden
   ├── idt_init()      256 Interrupt-Vektoren registrieren
   ├── pic_remap()      IRQs auf 0x20-0x2F verschieben
   ├── pit_init(100Hz)  Timer konfigurieren
   │
   ├── pmm_init()       Physical Memory Manager
   ├── scheduler_init() Round-Robin Scheduler
   │
   └── sti              Interrupts aktivieren → Preemption läuft
```

## Projektstruktur

```
kernel_os/
├── Makefile              Build-System (Cross-Compile + ISO via GRUB)
├── grub.cfg              Bootloader-Konfiguration
├── include/
│   ├── pmm.h             Physical Memory Manager
│   ├── vmm.h             Virtual Memory Manager
│   ├── task.h            Process Control Block
│   ├── scheduler.h       Round-Robin Scheduler
│   ├── gdt.h             Global Descriptor Table + TSS
│   ├── idt.h             Interrupt Descriptor Table
│   ├── pic.h             8259 PIC
│   ├── pit.h             Programmable Interval Timer
│   └── interrupts.h      Exception/IRQ-Handler-Deklarationen
└── src/
    ├── boot.S             Multiboot2 + Long-Mode-Transition
    ├── linker.ld          Speicher-Layout
    ├── kernel_main.cpp    Einstiegspunkt, initialisiert alle Subsysteme
    ├── pmm.cpp / vmm.cpp
    ├── task.cpp / scheduler.cpp / context_switch.S
    ├── gdt.cpp / gdt_flush.S
    ├── idt.cpp / idt_load.S / isr_stubs.S
    ├── pic.cpp / pit.cpp
    └── interrupts.cpp     exception_handler() + irq_handler()
```

## Screenshots

**Boot-Vorgang** (Multiboot2 → Long Mode → Subsystem-Initialisierung):

![Boot](screenshots/01_boot.png)

**Präemptiver Scheduler in Aktion** – zwei Kernel-Threads laufen unabhängig
voneinander und werden automatisch per Timer-Interrupt (100 Hz) umgeschaltet.
Der grüne Zähler (Thread A) und der rote Zähler (Thread B) laufen
gleichzeitig weiter, ohne dass sich die Threads gegenseitig blockieren:

![Scheduler läuft](screenshots/02_scheduler_running_clean_build.png)

> Getestet mit: `qemu-system-x86_64 8.2.2`, gebaut mit `g++ 13` auf Ubuntu 24.04.
> Build ist komplett warnungsfrei (`-Wall -Wextra`).

## Build & Ausführen

**Voraussetzungen** (Ubuntu/Debian):
```bash
sudo apt install build-essential nasm xorriso grub-pc-bin grub-common qemu-system-x86
```

**Bauen und in QEMU starten:**
```bash
make run
```

**Nur die ISO bauen:**
```bash
make iso
```

**Aufräumen:**
```bash
make clean
```

## Technische Details & bewusste Design-Entscheidungen

- **Kein User-Mode bisher** – aktuell laufen nur Kernel-Threads. GDT/TSS sind aber bereits vorbereitet für Ring3-Support.
- **Identity-Mapping im Kernel** – physischer Speicher wird 1:1 gemappt (vereinfachtes Modell für die frühe Entwicklungsphase; ein Higher-Half-Direct-Map-Ansatz wäre der nächste Ausbauschritt).
- **Kooperative vs. präemptive Umschaltung** – der Scheduler wird automatisch alle 10ms per Timer-Interrupt getriggert (`pit_init(100)`), kann aber auch manuell über `scheduler_tick()` angestoßen werden.
- **Kein dynamischer Heap im Kernel** – Tasks werden aus einem statischen Pool alloziert, um komplett ohne Standardbibliothek auszukommen.

## Roadmap

- [x] `task_yield()` für kooperatives Testen ohne Timer-Interrupt
- [x] Sichtbarer Scheduler-Beweis (zwei Threads schreiben live auf den Bildschirm)
- [ ] Tastatur-Treiber (IRQ1 ist bereits vorbereitet)
- [ ] User-Mode-Support (Ring 3) + Syscall-Interface
- [ ] Einfaches Dateisystem
- [ ] ELF-Loader für User-Programme
- [ ] SMP-Support (Multi-Core)

## Lizenz

MIT License, siehe [LICENSE](LICENSE).

## Motivation

Dieses Projekt entstand, um systemnahe Konzepte praktisch zu verstehen: Speicherverwaltung, Prozess-Scheduling, Interrupt-Handling und die x86_64-Architektur auf Hardware-Ebene – ohne die Abstraktionen eines bestehenden Betriebssystems.
