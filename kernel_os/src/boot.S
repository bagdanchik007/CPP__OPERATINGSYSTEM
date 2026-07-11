# ============================================================
# boot.S - x86_64 Boot-Prozess: Multiboot2 -> Protected Mode
#          -> Long Mode -> Sprung in kernel_main()
#
# GRUB (oder ein anderer Multiboot2-Bootloader) lädt uns im
# 32-bit Protected Mode. x86_64-CPUs starten NICHT direkt im
# 64-bit-Modus -> wir müssen den Übergang manuell durchführen.
# ============================================================

.set MB2_MAGIC,        0xE85250D6      # Multiboot2 Magic Number
.set MB2_ARCH,          0               # 0 = i386 Protected Mode
.set MB2_LENGTH,        (multiboot_header_end - multiboot_header)
.set MB2_CHECKSUM,      -(MB2_MAGIC + MB2_ARCH + MB2_LENGTH)

# ------------------------------------------------------------
# Multiboot2-Header: GRUB sucht danach in den ersten 32KiB des
# Kernel-Images, um zu erkennen "das hier ist ein bootbarer Kernel".
# ------------------------------------------------------------
.section .multiboot
.align 8
multiboot_header:
    .long MB2_MAGIC
    .long MB2_ARCH
    .long MB2_LENGTH
    .long MB2_CHECKSUM

    # End-Tag (Pflicht, markiert das Ende der Header-Tags)
    .align 8
    .word 0     # type
    .word 0     # flags
    .long 8     # size
multiboot_header_end:

# ------------------------------------------------------------
# BSS: Page Tables + Stack. .align 4096, weil Page Tables auf
# 4KiB-Grenzen liegen müssen.
# ------------------------------------------------------------
.section .bss
.align 4096
pml4_table:
    .skip 4096
pdpt_table:
    .skip 4096
pd_table:
    .skip 4096

.align 16
stack_bottom:
    .skip 16384         # 16 KiB initialer Boot-Stack
stack_top:

# ------------------------------------------------------------
# 32-bit Einstiegspunkt (das ist der Zustand, in dem GRUB uns übergibt)
# ------------------------------------------------------------
.section .text
.code32
.global _start
.type _start, @function

_start:
    # GRUB übergibt uns in EAX das Multiboot2-Magic und in EBX
    # einen Zeiger auf die Boot-Informationsstruktur. Wir sichern
    # sie, falls kernel_main() sie später braucht (z.B. Memory Map!).
    mov %eax, %edi          # wird später beim 64-bit Aufruf gebraucht
    mov %ebx, %esi

    mov $stack_top, %esp
    mov %esp, %ebp

    call check_multiboot
    call check_cpuid
    call check_long_mode

    call setup_page_tables
    call enable_paging

    lgdt (gdt64_pointer)

    # Far Jump in den 64-bit Code-Segment-Selektor der neuen GDT.
    # Das lädt CS neu und ist der eigentliche Sprung in den 64-bit-Modus.
    ljmp $gdt64_code_offset, $long_mode_start

# --- Sanity-Check: wurden wir wirklich per Multiboot2 gebootet? ---
check_multiboot:
    cmp $0x36d76289, %eax      # Multiboot2 Bootloader-Magic in EAX
    jne .no_multiboot
    ret
.no_multiboot:
    mov $'0', %al
    jmp error_halt

# --- Sanity-Check: unterstützt die CPU überhaupt CPUID? ---
check_cpuid:
    pushfl
    pop %eax
    mov %eax, %ecx
    xor $(1 << 21), %eax       # ID-Bit im FLAGS-Register umkippen
    push %eax
    popfl
    pushfl
    pop %eax
    push %ecx
    popfl
    cmp %ecx, %eax
    je .no_cpuid
    ret
.no_cpuid:
    mov $'1', %al
    jmp error_halt

# --- Sanity-Check: unterstützt die CPU Long Mode (64-bit)? ---
check_long_mode:
    mov $0x80000000, %eax
    cpuid
    cmp $0x80000001, %eax
    jb .no_long_mode

    mov $0x80000001, %eax
    cpuid
    test $(1 << 29), %edx      # LM-Bit
    jz .no_long_mode
    ret
.no_long_mode:
    mov $'2', %al
    jmp error_halt

# ------------------------------------------------------------
# Identity-Mapping der ersten 1 GiB via 2MiB-Huge-Pages.
# Reicht für den Kernel-Start locker aus; dein VMM (vmm.cpp)
# übernimmt später das feingranulare Mapping mit 4KiB-Seiten.
# ------------------------------------------------------------
setup_page_tables:
    # PML4[0] -> PDPT
    mov $pdpt_table, %eax
    or $0b11, %eax              # Present + Writable
    mov %eax, (pml4_table)

    # PDPT[0] -> PD
    mov $pd_table, %eax
    or $0b11, %eax
    mov %eax, (pdpt_table)

    # PD: 512 Einträge, jeder eine 2MiB Huge Page -> deckt 1 GiB ab
    mov $0, %ecx
.map_pd_loop:
    mov $0x200000, %eax         # 2 MiB
    mul %ecx
    or $0b10000011, %eax        # Present + Writable + Huge Page (Bit 7)
    mov %eax, pd_table(,%ecx,8)

    inc %ecx
    cmp $512, %ecx
    jne .map_pd_loop
    ret

enable_paging:
    # CR3 = physische Adresse der PML4
    mov $pml4_table, %eax
    mov %eax, %cr3

    # PAE aktivieren (Pflicht für Long Mode)
    mov %cr4, %eax
    or $(1 << 5), %eax
    mov %eax, %cr4

    # Long Mode im EFER-MSR aktivieren (LME-Bit)
    mov $0xC0000080, %ecx
    rdmsr
    or $(1 << 8), %eax
    wrmsr

    # Paging aktivieren (PG-Bit in CR0) -> ab hier "Compatibility Mode"
    mov %cr0, %eax
    or $(1 << 31), %eax
    mov %eax, %cr0
    ret

# --- Fehlerfall: Zeichencode in AL auf Bildschirm schreiben, dann anhalten ---
error_halt:
    movl $0x4f524f45, (0xb8000)   # "ER" in Weiß auf Rot (VGA-Textmodus)
    movl $0x4f3a4f52, (0xb8004)   # "R:"
    movl $0x4f204f20, (0xb8008)
    mov %al, (0xb800a)
    hlt

# ------------------------------------------------------------
# Minimale GDT für Long Mode.
# Im 64-bit-Modus werden Base/Limit der Segmente von der CPU
# ignoriert (flaches Speichermodell) - wir brauchen die GDT NUR,
# damit CS ein gültiges 64-bit-Code-Segment referenziert.
# ------------------------------------------------------------
.section .rodata
.align 8
gdt64:
    .quad 0                                  # Pflicht-Nulldeskriptor
.gdt64_code:
    .quad (1<<43) | (1<<44) | (1<<47) | (1<<53)  # Code: executable, code/data, present, 64-bit
.gdt64_data:
    .quad (1<<44) | (1<<47) | (1<<41)            # Data: code/data, present, writable
gdt64_end:

.set gdt64_code_offset, .gdt64_code - gdt64
.set gdt64_data_offset, .gdt64_data - gdt64

gdt64_pointer:
    .word gdt64_end - gdt64 - 1   # Limit
    .quad gdt64                    # Base

# ------------------------------------------------------------
# 64-bit Einstiegspunkt: ab hier sind wir "wirklich" im Long Mode.
# ------------------------------------------------------------
.section .text
.code64
long_mode_start:
    # Datensegment-Register auf den neuen 64-bit-Datenselektor setzen.
    # (CS wurde bereits durch den ljmp oben gesetzt.)
    mov $gdt64_data_offset, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    mov $stack_top, %rsp
    mov %rsp, %rbp

    # edi/esi wurden oben mit Multiboot-Magic/Info-Pointer belegt ->
    # bei einem 64-bit Aufruf werden die unteren 32 Bit automatisch
    # in rdi/rsi übernommen (System V AMD64 Calling Convention).
    call kernel_main

    # Falls kernel_main() jemals zurückkehrt (sollte es nicht) ->
    # CPU anhalten statt undefiniertes Verhalten laufen zu lassen.
.hang:
    cli
    hlt
    jmp .hang

.size _start, . - _start

.section .note.GNU-stack,"",@progbits
