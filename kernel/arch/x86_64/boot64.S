.globl long_mode_start
.extern _init
.extern kmain

.section .text
.code64
.type long_mode_start, @function
long_mode_start:
    # Load null into all data segment registers
    mov $0, %ax
    mov %ax, %ss
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    # Call global constructors
    call _init

    # Start C code
    call kmain

	hlt
.size long_mode_start, . - long_mode_start