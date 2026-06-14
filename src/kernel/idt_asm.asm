extern isr_handler
global idt_flush

section .text
bits 64

idt_flush:
    ; Load the new IDT pointer.
    lidt [rdi]
    ret

%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push qword 0 ; dummy error code
    push qword %1 ; interrupt number
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push qword %1 ; interrupt number
    jmp isr_common_stub
%endmacro

; Define ISRs
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

; IRQs
%assign i 32
%rep 16
    ISR_NOERRCODE i
    %assign i i+1
%endrep

isr_common_stub:
    ; Save caller state before entering the C interrupt handler.
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Detect ring 3 entries so swapgs can be applied correctly.
    test qword [rsp + 18*8], 3 ; CS is 18th qword from top of stack
    jz .kernel_entry
    swapgs
.kernel_entry:

    ; Hand the full interrupt frame to C.
    mov rdi, rsp
    call isr_handler
    
    ; The C handler may return a different stack pointer after scheduling.
    mov rsp, rax

    ; If returning to user mode, restore user data segments and swapgs back.
    test qword [rsp + 18*8], 3
    jz .kernel_exit
    
    ; Load User Data segments
    mov ax, 0x1B ; User Data (entry 3, 0x18 | 3)
    mov ds, ax
    mov es, ax
    
    swapgs
.kernel_exit:

    ; Restore saved registers from the selected return frame.
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Drop the synthetic interrupt metadata and return to the interrupted RIP.
    add rsp, 16
    iretq
