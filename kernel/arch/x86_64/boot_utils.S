.globl error
.globl multiboot_error_message
.globl cpuid_error_message
.globl long_mode_error_message
.code32

#include "memory.h"
#define VGA_MEMORY	0xB8000

.section .text
.type error, @function
error:
    mov %eax, %esi
    call print
	jmp freeze
.size error, . - error

.type print, @function
print:
    pushl %eax
    pushl %esi
    pushl %edi

    mov %eax, %esi
	mov $VGA_MEMORY, %edi
b1:
	# Load the current character into a register (e.g., %al)
    movb (%esi), %al

    # Check if we've reached the null terminator
    cmpb $0, %al
    je b2   # If equal, end the loop

    # Process the current character (e.g., print it)
	movb %al, (%edi)

    # Move to the next character
	inc %edi
	inc %edi
    inc %esi

    # Continue looping
    jmp b1

b2:
    pop %edi
    pop %esi
    pop %eax
	ret
.size print, . - print

.type freeze, @function
freeze:
	cli
	hlt
	jmp freeze
.size freeze, . - freeze

.section .rodata
multiboot_error_message = . - KERNEL_ADDRESS
    .string "Error: Invalid multiboot"
cpuid_error_message = . - KERNEL_ADDRESS
    .string "Error: CPU ID not supported"
long_mode_error_message = . - KERNEL_ADDRESS
    .string "Error: Long mode not supported"