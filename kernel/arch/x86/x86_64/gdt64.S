.globl gdt64
.globl code
.globl data
.globl tss
.globl gdt64_pointer
.globl gdt64_pointer_high

#include <asm/memory.h>

# GDT Access Byte bits
#define GDT_ACCESS_BYTE_P_BIT 			0b10000000
#define GDT_ACCESS_BYTE_DPL_KERNEL_BIT 	0b00000000
#define GDT_ACCESS_BYTE_DPL_USER_BIT 	0b01100000
#define GDT_ACCESS_BYTE_S_BIT 			0b00010000
#define GDT_ACCESS_BYTE_E_BIT 			0b00001000
#define GDT_ACCESS_BYTE_RW_BIT 			0b00000010

# GDT Flags bits
#define GDT_FLAGS_LONG_MODE				0b0010

# Global Descriptor Table x64 bits
# Order must match GDT_INDEX_* in <x86/gdt.h>
.section .data
    .align 16
gdt64:

    # Null Descriptor
    null = . - gdt64
        .quad 0                              # Null descriptor

    # Kernel Code Descriptor (index 1, selector 0x08)
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

	# Kernel Data Descriptor (index 2, selector 0x10)
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

	# User Data Descriptor (index 3, selector 0x1B)
	user_data = . - gdt64
		.word 0             			    # Limit (low)
        .word 0             			    # Base (low)
        .byte 0     			            # Base (middle)
        .byte ( \
			GDT_ACCESS_BYTE_P_BIT | \
			GDT_ACCESS_BYTE_DPL_USER_BIT | \
			GDT_ACCESS_BYTE_S_BIT | \
			GDT_ACCESS_BYTE_RW_BIT \
		)			                 		# Access Byte
        .byte (GDT_FLAGS_LONG_MODE << 4)	# Limit + Flags
        .byte 0                				# Base (high)

    # User Code Descriptor (index 4, selector 0x23)
    user_code = . - gdt64
        .word 0             			    # Limit (low)
        .word 0             			    # Base (low)
        .byte 0     			            # Base (middle)
        .byte ( \
			GDT_ACCESS_BYTE_P_BIT | \
			GDT_ACCESS_BYTE_DPL_USER_BIT | \
			GDT_ACCESS_BYTE_S_BIT | \
			GDT_ACCESS_BYTE_E_BIT | \
			GDT_ACCESS_BYTE_RW_BIT \
		)			                 		# Access Byte
        .byte (GDT_FLAGS_LONG_MODE << 4)	# Limit + Flags
        .byte 0                				# Base (high)

    # Task State Segment (index 5-6, selector 0x28)
    # 16-byte system descriptor, filled in at runtime by tss_init()
    tss = . - gdt64
        .quad 0
        .quad 0
gdt64_end:

# GDT x64 bits Pointer 
gdt64_pointer:
	.word . - gdt64 - 1 					# Length
	.long gdt64 - KERNEL_VIRTUAL_ADDRESS	# Address

# Same GDT, but with its virtual address (8-byte base), usable after
# the identity map is removed
gdt64_pointer_high:
	.word gdt64_pointer - gdt64 - 1			# Length
	.quad gdt64								# Address