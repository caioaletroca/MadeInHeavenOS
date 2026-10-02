#include <mm/boot_alloc.h>
#include <panic.h>
#include <string.h>
#include <util.h>

static uintptr_t boot_next;
static uintptr_t boot_end;
static int boot_sealed;

void boot_alloc_init(physaddr_t start, size_t size)
{
    boot_next = (uintptr_t)phys_to_kern(start);
    boot_end = boot_next + size;
}

void *boot_alloc(size_t size, size_t alignment)
{
    if (boot_sealed)
        panic("boot_alloc: called after kmalloc_init()");

    uintptr_t address = ALIGN_UP(boot_next, alignment);

    if (address + size > boot_end)
        panic("boot_alloc: reservation exhausted (%u bytes requested)", (unsigned int)size);

    boot_next = address + size;
    memset((void *)address, 0, size);

    return (void *)address;
}

void boot_alloc_seal(void)
{
    boot_sealed = 1;
}