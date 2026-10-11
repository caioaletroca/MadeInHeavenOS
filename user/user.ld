/*
 * User programs: linked at USER_BASE (4 MiB, page 0 stays unmapped).
 * One PT_LOAD per permission set, each starting on its own page, so no two
 * segments share a page and the loader maps each with exactly its flags.
 */
ENTRY(_start)

PHDRS
{
    text   PT_LOAD FLAGS(5);   /* R + X */
    rodata PT_LOAD FLAGS(4);   /* R */
    data   PT_LOAD FLAGS(6);   /* R + W */
}

SECTIONS
{
    . = 0x400000;

    .text : { *(.text*) } :text

    .rodata : ALIGN(4096) { *(.rodata*) } :rodata

    .data : ALIGN(4096) { *(.data*) } :data

    /*
     * Constructors and destructors of global objects: arrays of function
     * pointers that libc's startup walks before main and exit walks after.
     * Prioritized entries first, in priority order. GCC may emit .ctors/.dtors
     * instead of .init_array/.fini_array depending on how it was configured:
     * both are collected into the same arrays.
     */
    .init_array : ALIGN(8)
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP(*(.init_array .ctors))
        PROVIDE_HIDDEN(__init_array_end = .);
    } :data

    .fini_array : ALIGN(8)
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
        KEEP(*(.fini_array .dtors))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } :data

    .bss : { *(.bss*) *(COMMON) } :data

    /* No unwinder, debugger notes or toolchain comments in the image */
    /DISCARD/ : { *(.eh_frame*) *(.note*) *(.comment) }
}
