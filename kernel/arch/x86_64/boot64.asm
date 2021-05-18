global long_mode_start
extern _init
extern kmain 

section .text
bits 64
long_mode_start:
    ; Load null into all data segment registers
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Call global constructors
    call _init

    ; Start C code
    call kmain

	hlt