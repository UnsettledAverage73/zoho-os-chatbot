[bits 64]

extern syscall_handler_c
global syscall_entry

section .text

syscall_entry:
    ; On entry:
    ; RCX = return RIP
    ; R11 = saved RFLAGS
    ; RAX = syscall number
    ; RDI, RSI, RDX, R10, R8, R9 = arguments
    
    ; Enter the kernel GS base and switch to the current CPU stack.
    swapgs
    mov [gs:0x00], rsp   ; Save user RSP
    mov rsp, [gs:0x08]   ; Load kernel RSP
    
    ; Save the user-facing return context.
    push qword [gs:0x00] ; User RSP
    push r11             ; User RFLAGS
    push rcx             ; User RIP
    
    ; Save general-purpose registers before calling into C.
    push rax
    push rdi
    push rsi
    push rdx
    push r10
    push r8
    push r9
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    
    ; Rearrange syscall arguments into the C ABI registers.
    mov r9, r8
    mov r8, r10
    mov rcx, rdx
    mov rdx, rsi
    mov rsi, rdi
    mov rdi, rax
    
    call syscall_handler_c
    
    ; Preserve the C return value in the saved register frame.
    mov [rsp + 12*8], rax ; Overwrite saved RAX with return value
    
    ; Restore the registers that existed before the syscall.
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    pop r9
    pop r8
    pop r10
    pop rdx
    pop rsi
    pop rdi
    pop rax ; This is now the return value

    ; Restore the user return state expected by SYSRET.
    pop rcx ; RIP
    pop r11 ; RFLAGS
    pop rsp ; RSP
    
    ; Return to user mode.
    swapgs
    o64 sysret
