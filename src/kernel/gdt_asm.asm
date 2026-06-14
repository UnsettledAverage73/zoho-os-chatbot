global gdt_load
global tss_load

section .text
bits 64

gdt_load:
    ; Load the per-CPU GDT descriptor.
    lgdt [rdi]
    ; Reload the data segments after installing the new GDT.
    mov ax, 0x10 ; Kernel Data (2nd entry, 0x10)
    mov ds, ax
    mov es, ax
    mov ss, ax
    
    ; Far return to reload CS from the new GDT.
    pop rdi ; Return address
    mov rax, 0x08 ; Kernel Code (1st entry, 0x08)
    push rax
    push rdi
    retfq

tss_load:
    ; Load the TSS selector for privilege transitions.
    mov ax, 0x28 ; TSS entry is at 5 * 8 = 0x28
    ltr ax
    ret
