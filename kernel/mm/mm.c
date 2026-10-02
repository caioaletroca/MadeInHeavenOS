#include <mm/mm.h>
#include <mm/mmap.h>
#include <mm/kmalloc.h>

void mm_init(const boot_info_t *info)
{
    mmap_init(info);
    kmalloc_init();
}