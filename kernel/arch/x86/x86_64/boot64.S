.globl boot64
.extern _init
.extern kmain

# Page tables
.extern page_table_l4

# GDT variables
.extern data
.extern gdt64_pointer_high

.code64

#include <asm/memory.h>

.section .text
.type boot64, @function
boot64:
    # Indirect jump to for non-relative jump
    movabsq $boot64_high, %rax
	jmp *%rax
.size boot64, . - boot64

.type boot64_high, @function
boot64_high:
    # Adjust the stack pointer to be in upper memory
	movq $KERNEL_VIRTUAL_ADDRESS, %rax
	addq %rax, %rsp

    # GDT data segment into all data segment registers
    mov $data, %ax
    mov %ax, %ss
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    # Point GDTR at the GDT's virtual address; the current GDTR base is
    # physical and stops resolving once low memory is unmapped
	lgdt gdt64_pointer_high(%rip)
	
	# Unmap the kernel from low memory. L4[0] and L4[511] share the same
	# L3 table, so only the L4 entry may be cleared. Note the address
	# (leaq), not the contents (movq without '$'), of page_table_l4.
	leaq page_table_l4(%rip), %rax
    movq $0, (%rax)

    # Flush whole TLB
    mov %cr3, %rax
    mov %rax, %cr3

	# Call global constructors
    call _init

    # Start C code
    mov %rbx, %rdi
    call kmain

    hlt
.size boot64_high, . - boot64_high