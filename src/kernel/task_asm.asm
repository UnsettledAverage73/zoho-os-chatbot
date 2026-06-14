global switch_context

section .text
bits 64

; switch_context(void** old_rsp, void* new_rsp)
switch_context:
    ; Save callee-saved registers from the outgoing task.
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; Store the current stack pointer into *old_rsp.
    mov [rdi], rsp

    ; Switch to the incoming task stack.
    mov rsp, rsi

    ; Restore callee-saved registers for the incoming task.
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    ret
