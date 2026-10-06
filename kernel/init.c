#include <exec/exec.h>
#include <sched/process.h>
#include <driver/console.h>
#include <mm/address_space.h>
#include <boot_info.h>
#include <string.h>
#include <kprintf.h>
#include <panic.h>

/**
 * Find a boot module by name.
 *
 * @param info Pointer to the boot information structure.
 * @param name Name of the boot module to find.
 * @return Pointer to the boot module if found, NULL otherwise.
 */
static const boot_module_t *boot_module_find(const boot_info_t *info, const char *name)
{
    for (size_t i = 0; i < info->module_count; i++)
    {
        if (strcmp(info->modules[i].name, name) == 0)
            return &info->modules[i];
    }
    return NULL;
}

void init_start(const boot_info_t *info)
{
    const boot_module_t *module = boot_module_find(info, "hello");
    if (module == NULL)
        panic("init_start: Kernel module not found\n");

    address_space_t *space;
    uintptr_t entry;
    if (exec_load(phys_to_kern(module->start), module->end - module->start, &space, &entry) < 0)
        panic("init_start: Failed to load executable\n");

    process_t *process = process_create(space);
    if (process == NULL)
        panic("init_start: Failed to create process\n");

    for (int i = 0; i < 3; i++)
        if (process_fd_install(process, console_file()) < 0)
            panic("init_start: Failed to install console file descriptor\n");

    if (process_start(process, entry, USER_STACK_TOP) < 0)
        panic("init_start: Failed to start process\n");

    int status = process_wait(process);

    process_release(process);
}