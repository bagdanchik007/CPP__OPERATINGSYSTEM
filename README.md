# OPERATING SYSTEM
# Memory Manager, Process Management & Scheduler (x86_64)

## Projektstruktur

```
kernel_os/
├── include/
│   ├── pmm.h          Physical Memory Manager (Bitmap Allocator)
│   ├── vmm.h          Virtual Memory Manager (4-Level Paging)
│   ├── task.h         Task/PCB-Struktur + CPU-Kontext
│   └── scheduler.h     Round-Robin Scheduler
└── src/
    ├── pmm.cpp
    ├── vmm.cpp
    ├── task.cpp
    ├── scheduler.cpp
    └── context_switch.S   x86_64 Assembly für den Register-Wechsel
```

## Wie der Timer-Interrupt den Scheduler triggert

Der Ablauf, den du in deinem Interrupt-Setup (IDT + PIC/APIC) implementieren musst:

1. **PIT oder APIC-Timer konfigurieren**, damit er periodisch (z.B. alle 10 ms)
   einen Interrupt auf Vektor `IRQ0` (klassisch) oder einen APIC-Timer-Vektor
   auslöst.

2. **Interrupt-Handler in der IDT eintragen** (typischerweise als
   `naked`-Assembly-Stub, der zuerst *alle* Register auf den Stack rettet,
   die von einem C++-Aufruf potenziell zerstört würden — nicht zu verwechseln
   mit dem Task-Context-Switch! Das ist ein separater, kurzlebiger Save/Restore
   nur für die Interrupt-Behandlung selbst):

   ```
   timer_interrupt_stub:
       ; alle Register sichern (pusha-Äquivalent unter x86_64)
       call timer_interrupt_handler   ; C++ Funktion
       ; alle Register wiederherstellen
       ; End-of-Interrupt (EOI) an PIC/APIC senden!
       iretq
   ```

3. **In `timer_interrupt_handler()` (C++) einfach `scheduler_tick()` aufrufen.**
   Das ist die gesamte Kopplung zwischen Timer und Scheduler:

   ```cpp
   extern "C" void timer_interrupt_handler() {
       // EOI senden NICHT vergessen (sonst kommen nie wieder Timer-IRQs!)
       pic_send_eoi(0);
       scheduler_tick();
   }
   ```

4. **Wichtige Falle:** `scheduler_tick()` ruft `context_switch()` auf, welches
   per `ret` "zurückspringt" — aber eben in einen *anderen* Task hinein, der
   selbst mitten in seinem eigenen `iretq`-Rücksprung aus einem früheren
   Timer-Interrupt "eingefroren" war. Das bedeutet: dein Assembly-Stub für den
   Interrupt und dein `context_switch` müssen zueinander passende
   Stack-Layouts erzeugen, sonst landest du mit falschen Registern/Segmenten
   in User- oder Kernel-Space. Für den Anfang (**nur Kernel-Threads, kein
   User-Mode**) reicht das hier gezeigte einfache Modell aus PCB + `ret`-Trick
   völlig aus.

5. **Preemption vs. Kooperation:** Mit diesem Aufbau hast du bereits
   **präemptives** Multitasking — der Scheduler kann jeden Task nach Ablauf
   des Zeit-Slots unterbrechen, unabhängig davon, ob der Task "freiwillig"
   aufgibt. Für den Einstieg kannst du zusätzlich eine `task_yield()`-Funktion
   bauen, die manuell `scheduler_tick()` aufruft — nützlich zum Testen, bevor
   der Timer-Interrupt überhaupt läuft.

## Nächste sinnvolle Schritte

1. GDT + TSS aufsetzen (für sauberen Ring0/Ring3-Wechsel später)
2. IDT + Interrupt-Stubs für Timer + Keyboard
3. `task_yield()` für kooperatives Testen ohne Timer
4. Erste zwei Test-Kernel-Threads, die abwechselnd etwas auf den Bildschirm
   schreiben (klassischer "ping/pong"-Scheduler-Test)
5. Später: User-Mode-Support (Ring 3), Syscalls, ELF-Loader

## Bekannte Vereinfachungen (bewusst, für den Hobby-Einstieg)

- Physischer Speicher wird im Kernel als identity-mapped angenommen
  (`phys_to_virt` = Identität). Für einen echten Higher-Half-Kernel später
  durch ein festes HHDM-Offset ersetzen.
- Kernel-Stacks werden über mehrere `pmm_alloc_page()`-Aufrufe alloziert und
  als physisch zusammenhängend angenommen — für Produktionsqualität besser
  einzeln allozieren und explizit über `vmm_map_page()` mappen.
- Kein Locking/Synchronisation — sobald du SMP (mehrere CPU-Kerne) willst,
  brauchst du Spinlocks um die Ready-Queue.
