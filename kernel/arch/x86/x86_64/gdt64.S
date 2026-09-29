.globl gdt64
.globl code
.globl data
.globl tss
.globl gdt64_pointer

.extern tss64_size
.extern ist_stack1
.extern ist_stack2

#include "memory.h"

# GDT Access Byte bits
#define GDT_ACCESS_BYTE_P_BIT 			0b10000000
#define GDT_ACCESS_BYTE_DPL_KERNEL_BIT 	0b00000000
#define GDT_ACCESS_BYTE_DPL_HIGH_BIT 	0b00100000
#define GDT_ACCESS_BYTE_DPL_MID_BIT 	0b01000000
#define GDT_ACCESS_BYTE_DPL_USER_BIT 	0b01100000
#define GDT_ACCESS_BYTE_S_BIT 			0b00010000
#define GDT_ACCESS_BYTE_E_BIT 			0b00001000
#define GDT_ACCESS_BYTE_DC_BIT 			0b00000100
#define GDT_ACCESS_BYTE_RW_BIT 			0b00000010
#define GDT_ACCESS_BYTE_A_BIT 			0b00000001

# GDT Flags bits
#define GDT_FLAGS_GRANULARITY			0b1000
#define GDT_FLAGS_SIZE_FLAG				0b0100
#define GDT_FLAGS_LONG_MODE				0b0010

# TSS Access Byte bits
#define GDT_TSS_ACCESS_BYTE_TYPE_LDT          0x2
#define GDT_TSS_ACCESS_BYTE_TYPE_AVAILABLE    0x9
#define GDT_TSS_ACCESS_BYTE_TYPE_BUSY         0xB

# Global Descriptor Table x64 bits
.section .data
    .align 16
gdt64:

    # Null Descriptor
    null = . - gdt64
        .word 0               				# Limit (low)
        .word 0               				# Base (low)
        .byte 0               				# Base (middle)
        .byte 0               				# Access Byte
        .byte 0         				    # Limit + Flags
        .byte 0                				# Base (high)

    # Code Descriptor
    code = . - gdt64
        .word 0             			    # Limit (low)
        .word 0             			    # Base (low)
        .byte 0     			            # Base (middle)
        .byte ( \
			GDT_ACCESS_BYTE_P_BIT | \
			GDT_ACCESS_BYTE_S_BIT | \
			GDT_ACCESS_BYTE_E_BIT | \
			GDT_ACCESS_BYTE_RW_BIT \
		)			                 		# Access Byte
        .byte (GDT_FLAGS_LONG_MODE << 4)	# Limit + Flags
        .byte 0                				# Base (high)

	# Data Descriptor
	data = . - gdt64
		.word 0             			    # Limit (low)
        .word 0             			    # Base (low)
        .byte 0     			            # Base (middle)
        .byte ( \
			GDT_ACCESS_BYTE_P_BIT | \
			GDT_ACCESS_BYTE_S_BIT | \
			GDT_ACCESS_BYTE_RW_BIT \
		)			                 		# Access Byte
        .byte (GDT_FLAGS_LONG_MODE << 4)	# Limit + Flags
        .byte 0                				# Base (high)

    # Task Stack Segment
	tss = . - gdt64
		.word tss64_size                    # Limit
        .word 0                             # Base (low)
        .byte 0                             # Base (middle)
        .byte ( \
            GDT_TSS_ACCESS_BYTE_TYPE_AVAILABLE | \
            GDT_ACCESS_BYTE_P_BIT \
        )                                   # Access Byte
        .byte 0                             # Limit + Flags
        .byte 0                             # Base (high)
        .long 0                             # Base (higher)
        .long 0                             # Reserved

# GDT x64 bits Pointer 
gdt64_pointer:
	.word . - gdt64 - 1 					# Length
	.long gdt64 - KERNEL_VIRTUAL_ADDRESS	# Address