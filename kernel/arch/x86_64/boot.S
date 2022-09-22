.globl _start
# .extern long_mode_start

.section .text
.code32
_start:
	# The bootloader has loaded us into 32-bit protected mode on a x86 
	# machine. Interrupts are disabled. Paging is disabled. The processor 
	# state is as defined in the multiboot standard. The kernel has full 
	# control of the CPU. The kernel can only make use of hardware features 
	# and any code it provides as part of itself. There's no printf 
	# function, unless the kernel provides its own <stdio.h> header and a 
	# printf implementation. There are no security restrictions, no 
	# safeguards, no debugging mechanisms, only what the kernel provides 
	# itself. It has absolute and complete power over the 
	# machine. 

	# To set up a stack, we set the esp register to point to the top of our 
	# stack (as it grows downwards on x86 systems). This is necessarily done 
	# in assembly as languages such as C cannot function without a stack. 

	# mov $64, ah
	# int $0x10

	movl $0x07690748, 0xb8000

	# mov stack_top, %esp

	# call check_multiboot

	# call setup_page_tables
	# call enable_paging

	# lgdt [gdt64.pointer]
	# jmp gdt64.code_segment:long_mode_start

	hlt

# check_multiboot:
# 	cpm eax, 0x36D76289
# 	jne .no_multiboot
# 	ret
# .no_multiboot:
# 	mov al, "M"
# 	jmp error

# setup_page_tables:
# 	mov page_table_l3, %eax
# 	or $0b11, %eax			# Enable Present and R/W flags
# 	mov %eax, [page_table_l4]

# 	mov page_table_l2, %eax
# 	or $0b11, %eax			# Enable Present and R/W flags
# 	mov %eax, [page_table_l3]

# 	mov $0, %ecx				# Start counter

# .loop:
# 	mov $0x200000, %eax		# 2 MiB data
# 	mul %ecx
# 	or $0b10000011, %eax		# Enable Present, R/W flags and huge page
# 	mov %eax, [page_table_l2 + %ecx * 8]

# 	inc %ecx
# 	cmp $512, %ecx			# Check for all memory mapped
# 	jne .loop

# 	ret

# enable_paging:
# 	mov %eax, page_table_l4
# 	mov cr3, %eax

# 	# Enable PAE
# 	mov %eax, %cr4
# 	or %eax, 1 << 5
# 	mov %cr4, %eax

# 	# Enable long mode
# 	mov %ecx, 0xC0000080
# 	rdmsr
# 	or %eax, 1 << 8
# 	wrmsr

# 	# Enable paging
# 	mov %eax, %cr0
# 	or %eax, 1 << 31
# 	mov %cr0, %eax

# 	ret

# erro3r:
# 	mov byte [0xB8000], %al
# 	hlt

# # The multiboot standard does not define the value of the stack pointer register
# # (esp) and it is up to the kernel to provide a stack. This allocates room for a
# # small stack by creating a symbol at the bottom of it, then allocating 16384
# # bytes for it, and finally creating a symbol at the top. The stack grows
# # downwards on x86. The stack is in its own section so it can be marked nobits,
# # which means the kernel file is smaller because it does not contain an
# # uninitialized stack. The stack on x86 must be 16-byte aligned according to the
# # System V ABI standard and de-facto extensions. The compiler will assume the
# # stack is properly aligned and failure to align the stack will result in
# # undefined behavior.
# .section .bss
# 	.align 4096
# page_table_l4:
# 	.space 4096
# page_table_l3:
# 	.space 4096
# page_table_l2:
# 	.space 4096
# stack_bottom:
# 	.space 4096 * 4
# stack_top:

# .section .rodata
# gdt64:
# 	dq 0 # zero entry
# .code_segment: equ $ - gdt64
# 	dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53) 	# code segment
# .pointer:
# 	.short $ - gdt64 - 1 									# length
# 	dq gdt64 											# address
