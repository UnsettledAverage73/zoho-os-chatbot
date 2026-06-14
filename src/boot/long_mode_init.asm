global long_mode_start
extern kmain

section .text
bits 64
long_mode_start:
    ; Reset segment selectors to a clean 64-bit startup state.
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Visible marker that long-mode entry succeeded.
    mov rax, 0x0f4b0f4f
    mov [abs 0xb8000], rax

    ; Hand control to the real kernel initialization path.
    call kmain
    hlt
