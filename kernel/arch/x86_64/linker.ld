ENTRY(_start)
OUTPUT_FORMAT(elf64-x86-64)

physical = 1M;
offset = 0xFFFFFFFF80000000;
virtual = offset + physical;
 
SECTIONS
{
	. = virtual;
 
	/* _kernel_start = .;
	.multiboot :
	{
		*(.multiboot)
	}

	. += offset; */
	.rodata ALIGN(4K) : AT(ADDR(.rodata) - offset)
	{
		*(.multiboot)
		*(.rodata)
	}

	.text ALIGN(4K) : AT(ADDR(.text) - offset)
	{
		*(.text)
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
	_kernel_end = .;
}