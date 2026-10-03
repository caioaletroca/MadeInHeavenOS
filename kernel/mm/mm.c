#include <mm/mm.h>
#include <mm/mmap.h>
#include <mm/kmalloc.h>
#include <arch/mmu.h>

void mm_init(const boot_info_t *info)
{
    mmap_init(info);
    kmalloc_init();

    // Initialize the architecture-specific memory management unit (MMU)
    arch_mmu_init();
}