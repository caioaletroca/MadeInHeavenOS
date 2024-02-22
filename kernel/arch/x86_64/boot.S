.globl _start
.extern boot64
.extern gdt64
.extern code
.extern gdt64_pointer

#include "memory.h"
#define VGA_MEMORY	0xB8000

.macro outb port val
	mov $\val, %al
	mov $\port, %dx
	out %al, %dx
.endm

.section .text
.code32
.type _start, @function
_start:
	# Disable interrupts
	cli

	# Clear the direction flag
	cld

	mov $stack_bottom - KERNEL_ADDRESS, %esp
	
	call clear_screen

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

	

	# call check_multiboot
	call check_cpuid
	call check_long_mode

	call setup_page_tables
	call enable_paging

	# lgdt (pointer)
	# jmp $code, $long_mode_start

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

.type check_multiboot, @function
check_multiboot:
	# Magic number to validate multiboot
	cmp $0x36D76289, %eax
	jne no_multiboot
	ret
no_multiboot:
	# Sends 1 to error output
	mov $0x0731, %eax
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
	# Sends 2 to error output
	mov $0x0732, %eax
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
	# Sends 3 to error output
	mov $0x0733, %eax
	jmp error
.size check_long_mode, . - check_long_mode

setup_page_tables:
	mov $page_table_l3, %eax
	or $0b11, %eax			# Enable Present and R/W flags
	mov %eax, page_table_l4

	mov $page_table_l2, %eax
	or $0b11, %eax			# Enable Present and R/W flags
	mov %eax, page_table_l3

	mov $0, %ecx				# Start counter

1:
	mov $0x200000, %eax		# 2 MiB data
	mul %ecx
	or $0b10000011, %eax		# Enable Present, R/W flags and huge page
	# mov %eax, %es(page_table_l2, %ecx, 8)
	mov %eax, page_table_l2(, %ecx, 8)
	# mov %eax, [page_table_l2 + %ecx * 8]

	inc %ecx
	cmp $512, %ecx			# Check for all memory mapped
	jne 1b
	
	ret

enable_paging:
	mov $page_table_l4, %eax
	mov %eax, %cr3
	
	# Enable PAE
	mov %cr4, %eax
	or $(1 << 5), %eax
	mov %eax, %cr4
	
	# Enable long mode
	mov $0xC0000080, %ecx
	rdmsr
	or $(1 << 8), %eax
	wrmsr

	# Enable paging
	mov %cr0, %eax
	or $(1 << 31), %eax
	mov %eax, %cr0

	ret

.globl error
.type error, @function
error:
	mov $VGA_MEMORY, %ebx
	mov $0x0745, (%ebx)
	mov $0x0752, 2(%ebx)
	mov $0x0752, 4(%ebx)
	mov $0x073A, 6(%ebx)
	mov $0x0720, 8(%ebx)
	mov %eax, 10(%ebx)
	jmp freeze
.size error, . - error

.globl freeze
.type freeze, @function
freeze:
	cli
	hlt
	jmp freeze
.size freeze, . - freeze

.section .bss
	.align PAGE_SIZE
page_table_l4 = . - KERNEL_ADDRESS
	.space PAGE_SIZE
page_table_l3 = . - KERNEL_ADDRESS
	.space PAGE_SIZE
page_table_l2 = . - KERNEL_ADDRESS
	.space PAGE_SIZE
stack_top:
	.space PAGE_SIZE * 4
stack_bottom: