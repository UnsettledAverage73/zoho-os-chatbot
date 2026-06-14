[bits 16]

section .text

global trampoline_start
trampoline_start:
    cli
    mov ax, 0
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Load 32-bit GDT
    lgdt [0x8000 + gdt32_ptr - trampoline_start]

    ; Transition to protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:(0x8000 + trampoline32 - trampoline_start)

[bits 32]
trampoline32:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; Setup page tables (using the same ones as BSP)
    mov eax, [0x8000 + trampoline_p4_addr - trampoline_start]
    mov cr3, eax

    ; Enable PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; Enable long mode
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Enable paging
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ; Load 64-bit GDT
    lgdt [0x8000 + gdt64_ptr - trampoline_start]

    push dword 0x08
    push dword (0x8000 + trampoline64 - trampoline_start)
    retf

[bits 64]
trampoline64:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    ; Load kernel stack
    mov rsp, [0x8000 + trampoline_stack_addr - trampoline_start]

    ; Pass APIC ID in RDI (1st argument in System V AMD64 ABI)
    mov rdi, [0x8000 + trampoline_apic_id - trampoline_start]

    ; Jump to kernel entry point for APs
    mov rax, [0x8000 + trampoline_entry_addr - trampoline_start]
    jmp rax

align 16
gdt32:
    dq 0
    dq 0x00CF9A000000FFFF ; 32-bit code
    dq 0x00CF92000000FFFF ; 32-bit data
gdt32_ptr:
    dw $ - gdt32 - 1
    dd 0x8000 + gdt32 - trampoline_start

align 16
gdt64:
    dq 0
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53) ; 64-bit code
    dq (1<<41) | (1<<44) | (1<<47)           ; 64-bit data
gdt64_ptr:
    dw $ - gdt64 - 1
    dq 0x8000 + gdt64 - trampoline_start

global trampoline_p4_addr
global trampoline_stack_addr
global trampoline_entry_addr
global trampoline_apic_id

align 8
trampoline_p4_addr: dq 0
trampoline_stack_addr: dq 0
trampoline_entry_addr: dq 0
trampoline_apic_id: dq 0

global trampoline_end
trampoline_end:
