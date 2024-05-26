.globl boot64
.extern _init
.extern kmain

# Page tables
.extern page_table_l4
.extern page_table_l3

# GDT variables
.extern data
.extern tss
.extern gdt64_pointer
.extern tss64

.code64

#include "memory.h"

.section .text
.type boot64, @function
boot64:
    # Load TSS descriptor into GDT base address
    movq (gdt64 - KERNEL_ADDRESS), %rdi

    # Offset with the TSS, to get the TSS address
	addq $tss, %rdi

    # Get TSS descriptor address
	movq (tss64 - KERNEL_ADDRESS), %rax

    # Insert base (low), 2 bytes
	movw %ax, 2(%rdi)

    # Insert base (middle), 1 byte
	shrq $16, %rax
	movb %al, 4(%rdi)

    # Insert base high, 1 byte
	shrq $8, %rax
	movb %al, 7(%rdi)

    # Insert base highest, 4 bytes
	shrq $8, %rax
	movl %eax, 8(%rdi)

    # Load GDT again, but now since it's on 64 bits
    # the address will be correct
	lgdt (gdt64_pointer - KERNEL_ADDRESS)

    # Indirect jump to for non-relative jump
    movabsq $boot64_high, %rax
	jmp *%rax
.size boot64, . - boot64

.type boot64_high, @function
boot64_high:
    # Adjust the stack pointer to be in upper memory
	movq $KERNEL_ADDRESS, %rax
	addq %rax, %rsp

    # GDT data segment into all data segment registers
    mov $data, %ax
    mov %ax, %ss
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
	
	# Unmap the kernel from low memory
    movq page_table_l4, %rax
    movq $0, (%rax)
	movq page_table_l3, %rax
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