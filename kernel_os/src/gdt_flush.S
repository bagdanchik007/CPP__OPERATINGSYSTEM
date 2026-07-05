# ============================================================
# gdt_flush.S - lädt GDTR neu und aktualisiert Segment-Register
#
# extern "C" void gdt_flush(uint64_t gdt_ptr_addr);   // rdi
# extern "C" void tss_flush();
# ============================================================

.section .text
.code64

.global gdt_flush
.type gdt_flush, @function
gdt_flush:
    lgdt (%rdi)

    # Datensegment-Register neu laden (Kernel-Data-Selektor = 0x10)
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    # CS kann NICHT per mov gesetzt werden - nur über einen "far return"
    # oder far jump. Wir nutzen hier einen Far-Return-Trick:
    pop %rax                 # Rücksprungadresse vom Stack holen
    push $0x08                # Kernel-Code-Selektor
    push %rax                 # Rücksprungadresse wieder drauf
    lretq                     # "far return" -> lädt CS neu

.global tss_flush
.type tss_flush, @function
tss_flush:
    mov $0x28, %ax    # TSS-Selektor
    ltr %ax
    ret
