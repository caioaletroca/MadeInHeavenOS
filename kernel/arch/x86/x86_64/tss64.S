.globl tss64
.globl tss64_size
.extern ist_stack1
.extern ist_stack2

.section .data
    .align 16
tss64:
	.long 0					# Reserved
	.quad 0					# RSP0
	.quad 0					# RSP1
	.quad 0					# RSP2
	.quad 0					# Reserved
interrupt_stack_table:
	.quad ist_stack1		# IST1, NMI
	.quad ist_stack2		# IST2, Double Fault
	.quad 0					# IST3
	.quad 0					# IST4
	.quad 0					# IST5
	.quad 0					# IST6
	.quad 0					# IST7
	.quad 0					# Reserved
	.word 0					# Reserved
	.word 0					# I/O Map Base Address
tss64_size = . - tss64 - 1