.globl _start
.globl page_table_l3
.globl page_table_l4
.globl ist_stack1
.globl ist_stack2

.extern boot64

# GDT variables
.extern gdt64
.extern code
.extern gdt64_pointer

# Error Messages
.extern error
.extern multiboot_error_message
.extern cpuid_error_message
.extern long_mode_error_message
.extern sse_error_message

.code32

.macro outb port val
	mov $\val, %al
	mov $\port, %dx
	out %al, %dx
.endm

#include "memory.h"
#define VGA_MEMORY	0xB8000

.section .text
.type _start, @function
_start:
	# Disable interrupts
	cli

	# Clear the direction flag
	cld

	# Set stack pointer
	mov $stack_bottom - KERNEL_ADDRESS, %esp
	
	# Preserve multiboot information
	push %ebx

	call clear_screen

	call check_multiboot

	call set_cursor

	/* reset EFLAGS. */
	pushl $0
	popf

	call check_cpuid
	call check_long_mode
	call check_sse

	# Restore multiboot information into EBX	
	pop %ebx

	call enable_sse
	call setup_page_tables
	call enable_paging

	# Turn on GDT
	lgdt (gdt64_pointer - KERNEL_ADDRESS)

	# Jump to 64bits world
	ljmp $code, $(boot64 - KERNEL_ADDRESS)

	hlt
.size _start, . - _start

.type clear_screen, @function
clear_screen:
	# 80 * 25 characters on the screen
	movl $2000, %ecx
	jmp clear_end
clear:
	decl %ecx
	movw $0x0F20, VGA_MEMORY(, %ecx, 2)
clear_end:
	cmpl $0, %ecx
	jne clear
	ret
.size clear_screen, . - clear_screen

.type set_cursor, @function
set_cursor:
	# Set the cursor to blink
	outb 0x3D4, 0x09
	outb 0x3D5, 0x0F
	outb 0x3D4, 0x0B
	outb 0x3D5, 0x0F
	outb 0x3D4, 0x0A
	outb 0x3D5, 0x0E

	# Sets the cursor position to 0,1
	outb 0x3D4, 0x0F
	# Set cursor position to (row * 80) + col = (1 * 80) + 0 = 80
	outb 0x3D5, 80
	outb 0x3D4, 0x0E
	outb 0x3D5, 0x00

	ret
.size set_cursor, . - set_cursor

.type check_multiboot, @function
check_multiboot:
	# Magic number to validate multiboot
	cmpl $0x36D76289, %eax
	jne no_multiboot
	ret
no_multiboot:
	mov $multiboot_error_message, %eax
	jmp error
.size check_multiboot, . - check_multiboot

.type check_cpuid, @function
check_cpuid:
	# Copy Flags into EAX by stack
	pushf
	popl %eax

	# Copy to ECX as well for comparing later on
	movl %eax, %ecx

	# Flip the ID bit
	xorl $(1 << 21), %eax

	# Copy EAX to Flags by stack
	pushl %eax
	popf

	# Copy Flags back to EAX (with the flipped bit if CPUID is supported)
	pushf
	popl %eax

	# Restore Flags from the old version stored in ECX (flipping the ID bit back if
	# it was ever flipped)
	pushl %ecx
	popf

	# Compare EAX and ECX. If they are equal then that means the bit
	# wasn't flipped, and CPUID isn't supported
	xorl %ecx, %eax
	jz no_cpuid
	ret
no_cpuid:
	mov $cpuid_error_message, %eax
	jmp error
.size check_cpuid, . - check_cpuid

.type check_long_mode, @function
check_long_mode:
	# Set the A-register
	movl $0x80000000, %eax

	# CPU Identification
	cpuid

	# Compare value, if it's less, there is no long mode
	cmpl $0x80000001, %eax
	jb no_long_mode

	# Set the A-register again
	mov $0x80000001, %eax

	# CPU Identification
	cpuid

	# Test if the LM-bit, which is bit 29, is set in the D-register
	test $(1 << 29), %edx
	jz no_long_mode

	ret
no_long_mode:
	mov $long_mode_error_message, %eax
	jmp error
.size check_long_mode, . - check_long_mode

.type check_sse, @function
check_sse:
	mov $0x1, %eax
	cpuid
	test $(1 << 25), %edx
	jz no_sse
	ret
no_sse:
	mov $sse_error_message, %eax
	jmp error
.size check_sse, . - check_sse

.type enable_sse, @function
enable_sse:
	mov %cr0, %eax

	# Clear coprocessor emulation CR0.EM
	and $0xFFFB, %ax

	# Set coprocessor monitoring CR0.MP
	or $0x2, %ax

	mov %eax, %cr0
	mov %cr4, %eax

	# Set CR4.OSFXSR and CR4.OSXMMEXCPT at the same time
	or $(3 << 9), %ax

	mov %eax, %cr4

	ret
.size enable_sse, . - enable_sse

.type setup_page_tables, @function
setup_page_tables:
	# Configure Page Directory 4
	mov $(page_table_l3 - KERNEL_ADDRESS), %eax
	or $0b11, %eax									# Enable Present and R/W flags
	movl %eax, (page_table_l4 - KERNEL_ADDRESS)
	mov $511, %ecx
	movl %eax, page_table_l4 - KERNEL_ADDRESS(, %ecx, 8)

	# Configure Page Directory 3
	mov $(page_table_l2 - KERNEL_ADDRESS), %eax
	or $0b11, %eax									# Enable Present and R/W flags
	movl %eax, (page_table_l3 - KERNEL_ADDRESS)
	mov $510, %ecx
	movl %eax, page_table_l3 - KERNEL_ADDRESS(, %ecx, 8)

	mov $0, %ecx			# Start counter
1:
	mov $0x200000, %eax		# 2 MiB data
	mul %ecx
	or $0b10000011, %eax		# Enable Present, R/W flags and huge page
	mov %eax, page_table_l2 - KERNEL_ADDRESS(, %ecx, 8)

	inc %ecx
	cmp $512, %ecx			# Check for all memory mapped
	jne 1b
	
	ret
.size setup_page_tables, . - setup_page_tables

.type enable_paging, @function
enable_paging:
	# Ensure that paging is off
	movl    %cr0, %eax
	andl    $0x7fffffff, %eax
	movl    %eax, %cr0

	# Enable PAE
	mov %cr4, %eax
	or $(1 << 5), %eax
	mov %eax, %cr4

	# Load a Level4 Page Directory
	movl    $(page_table_l4 - KERNEL_ADDRESS), %eax
	mov     %eax, %cr3

	# Enable long mode
	mov $0xC0000080, %ecx
	rdmsr
	orl $(1 << 8), %eax
	wrmsr	

	# Enable paging
	movl %cr0, %eax
	orl $(1 << 31), %eax
	movl %eax, %cr0

	ret
.size enable_paging, . - enable_paging

.section .bss
	.align PAGE_SIZE
page_table_l4:
	.space PAGE_SIZE
page_table_l3:
	.space PAGE_SIZE
page_table_l2:
	.space PAGE_SIZE
stack_bottom:
	.space PAGE_SIZE * 4
stack_top:

ist_stack1:
	.space PAGE_SIZE
ist_stack2:
	.space PAGE_SIZE
