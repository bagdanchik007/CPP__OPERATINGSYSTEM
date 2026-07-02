# ============================================================
# context_switch.S - x86_64 Kernel Context Switch
#
# extern "C" void context_switch(Task* old_task, Task* new_task);
#
# System V AMD64 ABI Calling Convention:
#   rdi = 1. Argument = old_task
#   rsi = 2. Argument = new_task
#
# Task-Struct-Layout (siehe task.h) - relevante Offsets:
#   offsetof(Task, saved_rsp)  -> wir müssen den aktuellen rsp DORTHIN
#                                 schreiben und von dort für new_task laden.
#
# WICHTIG: Diese Offsets MÜSSEN mit der Struct-Definition in task.h
# übereinstimmen! Bei jeder Änderung an Task unbedingt hier anpassen,
# oder besser: über einen Build-Schritt (z.B. static_assert + Skript)
# automatisch generieren lassen.
# ============================================================

.section .text
.global context_switch
.type context_switch, @function

# Offset von "saved_rsp" innerhalb von struct Task, in Bytes.
# pid(4) + state(1) + padding(3) + kernel_stack_base(8) + kernel_stack_top(8)
# = 4 + 1 + 3 + 8 + 8 = 24
.equ TASK_SAVED_RSP_OFFSET, 24

context_switch:
    # --- Nur callee-saved Register sichern (System V ABI) ---
    # rax, rcx, rdx, rsi, rdi, r8-r11 sind caller-saved -> der Compiler
    # hat sie VOR diesem Funktionsaufruf bereits selbst gesichert, wenn
    # er sie über den Switch hinweg noch braucht. Wir müssen sie hier
    # nicht anfassen.
    push %rbp
    push %rbx
    push %r12
    push %r13
    push %r14
    push %r15

    # --- Aktuellen Stack-Pointer im ALTEN Task sichern ---
    # rdi = old_task (kann bei allererstem Aufruf NULL sein!)
    test %rdi, %rdi
    jz 1f                                  # old_task == NULL -> nichts sichern
    mov %rsp, TASK_SAVED_RSP_OFFSET(%rdi)  # old_task->saved_rsp = rsp

1:
    # --- Gesicherten Stack-Pointer des NEUEN Tasks laden ---
    # rsi = new_task
    mov TASK_SAVED_RSP_OFFSET(%rsi), %rsp  # rsp = new_task->saved_rsp

    # --- Register des neuen Tasks vom (neuen) Stack zurückholen ---
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    pop %rbp

    # --- Zurück zum Aufrufer springen ---
    # Da rsp jetzt auf den Stack von new_task zeigt, liegt hier entweder:
    #   a) die Rücksprungadresse aus dessen letztem context_switch-Aufruf
    #      (Task lief schon mal -> setzt exakt dort fort), oder
    #   b) die von task_create_kernel_thread() künstlich hinterlegte
    #      Adresse von entry_point (Task startet zum allerersten Mal).
    ret

.size context_switch, . - context_switch
