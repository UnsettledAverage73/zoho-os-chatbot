global start
extern long_mode_start

section .text
bits 32
start:
    ; Disable interrupts until a real stack and paging are ready.
    cli
    ; Install a temporary bootstrap stack.
    mov esp, stack_top
    
    ; Preserve Multiboot2 handoff registers before any calls clobber them.
    mov edi, eax
    mov esi, ebx

    ; VGA Marker '1'
    mov dword [0xb8000], 0x4f314f31

    ; Verify GRUB really handed us a Multiboot2 kernel entry.
    cmp edi, 0x36d76289
    jne .no_multiboot

    ; Make sure the CPU can execute CPUID before asking for features.
    call check_cpuid
    ; VGA Marker '2'
    mov dword [0xb8002], 0x4f324f32

    ; Confirm the CPU supports x86_64 long mode.
    call check_long_mode
    ; VGA Marker '3'
    mov dword [0xb8004], 0x4f334f33

    ; Build an identity-mapped paging tree for the bootstrap transition.
    call setup_page_tables
    ; Turn on paging, PAE, and long-mode enable.
    call enable_paging
    ; VGA Marker '4'
    mov dword [0xb8006], 0x4f344f34

    ; Load the 64-bit code segment and jump into long mode.
    lgdt [gdt64.pointer]
    ; edi and esi already contain eax and ebx
    jmp gdt64.code:long_mode_start

.no_multiboot:
    mov al, "M"
    jmp error

error:
    mov dword [0xb8000], 0x4f524f45
    mov dword [0xb8004], 0x4f3a4f52
    mov dword [0xb8008], 0x4f204f20
    mov byte  [0xb800a], al
    hlt

check_cpuid:
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    cmp eax, ecx
    je .no_cpuid
    ret
.no_cpuid:
    mov al, "C"
    jmp error

check_long_mode:
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_long_mode
    ret
.no_long_mode:
    mov al, "L"
    jmp error

setup_page_tables:
    ; Point the top-level table at the next level.
    mov eax, p3_table
    or eax, 0b11 ; present + writable
    mov [p4_table], eax

    mov ecx, 0
.map_p3_table:
    ; Wire the middle level to the large-page directory table.
    mov eax, 4096
    mul ecx
    add eax, p2_table
    or eax, 0b11
    mov [p3_table + ecx * 8], eax
    inc ecx
    cmp ecx, 4
    jne .map_p3_table

    mov ecx, 0
.map_p2_table:
    ; Identity-map low memory with 2 MiB huge pages.
    mov eax, 0x200000
    mul ecx
    or eax, 0b10000011
    mov [p2_table + ecx * 8], eax
    inc ecx
    cmp ecx, 2048
    jne .map_p2_table
    ret

enable_paging:
    ; Load the bootstrap page-table root.
    mov eax, p4_table
    mov cr3, eax
    ; Enable PAE in CR4.
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax
    ; Set EFER.LME to allow long mode.
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr
    ; Finally enable paging in CR0.
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax
    ret

section .bss
align 4096
p4_table:
    resb 4096
p3_table:
    resb 4096
p2_table:
    resb 4096 * 4
stack_bottom:
    resb 4096 * 4
stack_top:

section .rodata
gdt64:
    dq 0 ; zero entry
.code: equ $ - gdt64
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53) ; code segment
.pointer:
    dw $ - gdt64 - 1
    dq gdt64
