.globl long_mode_start
# .extern _init
# .extern kmain 

.section .text
.code64
.type long_mode_start, @function
long_mode_start:
    mov $0x0741, 0xB8000
    # Load null into all data segment registers
    mov $0, %ax
    mov %ax, %ss
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    # mov ax, 0
    # mov ss, ax
    # mov ds, ax
    # mov es, ax
    # mov fs, ax
    # mov gs, ax

    # Call global constructors
    # call _init

    # Start C code
    # call kmain

	hlt
.size long_mode_start, . - long_mode_start