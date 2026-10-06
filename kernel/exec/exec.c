#include <exec/exec.h>
#include <exec/elf.h>
#include <asm/memory.h>
#include <mm/address_space.h>
#include <syscall.h>

int exec_load(const void *image, size_t size, address_space_t **space, uintptr_t *entry)
{
    *space = address_space_create();
    if (*space == NULL)
        return -ENOMEM;

    int ret = elf_load(*space, image, size, entry);

    if (ret == 0 && address_space_map(*space, USER_STACK_TOP - PAGE_SIZE, PAGE_SIZE, MMU_WRITE) != 0)
        ret = -ENOMEM;

    if (ret != 0)
    {
        address_space_destroy(*space);
        *space = NULL;
    }

    return ret;
}