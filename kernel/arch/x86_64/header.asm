; Multiboot 2 specifications

; Constants
MAGIC           equ     0xE85250D6      ; Magic number for multiboot2
PROTECTED_MODE  equ     0               ; Specifies GRUB to start at Protected Mode
ZERO            equ     0x100000000     ; Number to handle checksum math

; Header specification
section .multiboot
header_start:
    dd MAGIC
    dd PROTECTED_MODE
    dd header_end - header_start
    dd ZERO - (MAGIC + PROTECTED_MODE + (header_end - header_start))

    ; Required end tag for multiboot
    dw 0    ; type
    dw 0    ; flags
    dd 8    ; tag size
header_end:
