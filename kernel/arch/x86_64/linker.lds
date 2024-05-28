#include "memory.h"

ENTRY(_start)
OUTPUT_FORMAT(elf64-x86-64)

physical = 1M;
offset = KERNEL_VIRTUAL_ADDRESS;
virtual = offset + physical;
 
SECTIONS
{
	. = virtual;
 
	_kernel_start = .;

	.rodata ALIGN(4K) : AT(ADDR(.rodata) - offset)
	{
		*(.multiboot)
		*(.rodata)
		*(.rodata*)
		*(.eh_frame)
	}

	.text ALIGN(4K) : AT(ADDR(.text) - offset)
	{
		*(.text)
		*(.text*)
	}
 
	.data ALIGN(4K) : AT(ADDR(.data) - offset)
	{
		*(.data)
	}
 
	.bss ALIGN(4K) : AT(ADDR(.bss) - offset)
	{
		*(COMMON)
		*(.bss)
	}
	_bss = .;
	
	_kernel_physical_end = . + 0x1000 - offset;
}