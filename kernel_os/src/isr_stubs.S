# ============================================================
# isr_stubs.S - Interrupt Service Routine Stubs
#
# Warum Assembly nötig ist: Ein C++-Interrupt-Handler kann nicht direkt
# als IDT-Ziel eingetragen werden, weil die CPU beim Interrupt-Eintritt
# NICHT automatisch alle Register sichert (nur rip/cs/rflags/rsp/ss) -
# und weil manche Exceptions einen Error-Code pushen und andere nicht,
# was den Stack sonst inkonsistent machen würde.
# ============================================================

.section .text
.code64

.extern exception_handler   # C++ Funktion in idt.cpp / fault_handler.cpp
.extern irq_handler         # C++ Funktion in idt.cpp / timer.cpp

# --- Makro für Exceptions OHNE Error-Code (CPU pusht keinen) ---
# Wir pushen selbst eine 0 als Platzhalter, damit InterruptFrame
# (siehe idt.h) IMMER dasselbe Layout hat, egal ob echter Error-Code
# oder nicht.
.macro ISR_NOERR num
.global isr\num
isr\num:
    push $0                # Dummy-Error-Code
    push $\num             # Interrupt-Nummer
    jmp isr_common_stub
.endm

# --- Makro für Exceptions MIT Error-Code (CPU pusht ihn automatisch) ---
.macro ISR_ERR num
.global isr\num
isr\num:
    push $\num             # Interrupt-Nummer (Error-Code liegt schon auf dem Stack)
    jmp isr_common_stub
.endm

# CPU-Exceptions 0-31. Nur diese pushen einen Error-Code:
# 8, 10, 11, 12, 13, 14, 17, 30
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31

# --- Gemeinsamer Stub für alle Exceptions ---
isr_common_stub:
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15

    mov %rsp, %rdi          # Zeiger auf InterruptFrame als 1. Argument
    call exception_handler

    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rbp
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax

    add $16, %rsp            # Interrupt-Nummer + Error-Code wieder wegräumen
    iretq

# --- Makro für Hardware-IRQs (nie Error-Code, aber eigene IDT-Vektoren) ---
.macro IRQ num, vector
.global irq\num
irq\num:
    push $0
    push $\vector
    jmp irq_common_stub
.endm

IRQ 0, 32    # Timer  (IDT-Vektor 0x20 nach PIC-Remap)
IRQ 1, 33    # Tastatur (IDT-Vektor 0x21)

irq_common_stub:
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15

    mov %rsp, %rdi           # Zeiger auf InterruptFrame als 1. Argument
    call irq_handler

    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rbp
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax

    add $16, %rsp
    iretq
