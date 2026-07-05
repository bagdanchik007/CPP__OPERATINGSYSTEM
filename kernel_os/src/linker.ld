/* linker.ld - Layout des Kernel-Images im Speicher */

ENTRY(_start)

/* Standard-Load-Adresse für Higher-Half-freie Hobby-Kernels.
   1 MiB ist die klassische Grenze, ab der x86 "freien" Speicher hat
   (darunter liegen BIOS/VGA-reservierte Bereiche). */
KERNEL_VIRT_BASE = 1M;

SECTIONS
{
    . = KERNEL_VIRT_BASE;

    /* Multiboot2-Header MUSS in den ersten 32KiB liegen -> ganz an den Anfang */
    .multiboot ALIGN(8) :
    {
        *(.multiboot)
    }

    .text ALIGN(4K) :
    {
        *(.text)
    }

    .rodata ALIGN(4K) :
    {
        *(.rodata)
    }

    .data ALIGN(4K) :
    {
        *(.data)
    }

    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss)
    }

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
