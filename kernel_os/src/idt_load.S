# ============================================================
# idt_load.S
# extern "C" void idt_load(uint64_t idt_ptr_addr);   // rdi
# ============================================================

.section .text
.code64

.global idt_load
.type idt_load, @function
idt_load:
    lidt (%rdi)
    ret

.section .note.GNU-stack,"",@progbits
