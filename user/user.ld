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
    .bss : { *(.bss*) *(COMMON) } :data

    /* No unwinder, debugger notes or toolchain comments in the image */
    /DISCARD/ : { *(.eh_frame*) *(.note*) *(.comment) }
}
