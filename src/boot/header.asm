section .multiboot_header
header_start:
    ; Multiboot2 magic number.
    dd 0xe85250d6
    ; Architecture 0 = protected-mode i386 handoff format.
    dd 0
    ; Total header length, including tags.
    dd header_end - header_start
    ; Checksum so the 32-bit sum of the header is zero.
    dd 0x100000000 - (0xe85250d6 + 0 + (header_end - header_start))

    ; Request a graphics framebuffer from GRUB.
    align 8
    dw 5    ; type
    dw 0    ; flags (0 = required)
    dd 20   ; size
    dd 1024 ; width
    dd 768  ; height
    dd 32   ; depth

    ; End tag
    align 8
    dw 0
    dw 0
    dd 8
header_end:
