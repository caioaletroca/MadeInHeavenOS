#include <exec/exec.h>
#include <sched/process.h>
#include <driver/console.h>
#include <mm/address_space.h>
#include <asm/memory.h>
#include <fs/file.h>
#include <boot_info.h>
#include <syscall.h>
#include <string.h>
#include <stdlib.h>
#include <kprintf.h>
#include <panic.h>

/*
 * TEMPORARY test runner: runs every boot module in grub.cfg order and checks
 * its exit status. Replaced by starting /sbin/init alone once spawn and wait
 * syscalls exist; running the tests then becomes a user-space job.
 */

/**
 * Run a boot module as a process with the console on fds 0-2 and wait for it.
 *
 * @param module The boot module to run as a process.
 * @param status Set to the exit status of the process on success.
 * @return 0 with *status set, or a negative errno if it could not be started.
 */
static int module_run(const boot_module_t *module, int *status)
{
    address_space_t *space;
    uintptr_t entry;
    int ret = exec_load(phys_to_kern(module->start), module->end - module->start, &space, &entry);
    if (ret < 0)
        return ret;

    // From here the process owns the space: releasing it destroys both
    process_t *process = process_create(space);
    if (process == NULL)
    {
        address_space_destroy(space);
        return -ENOMEM;
    }

    for (int i = 0; i < 3; i++)
    {
        file_t *console = console_file();
        if (process_fd_install(process, console) < 0)
        {
            file_put(console); // a failed install leaves the reference with the caller
            process_release(process);
            return -EMFILE;
        }
    }

    // process_start returns -1, not an errno: thread creation failed
    if (process_start(process, entry, USER_STACK_TOP) < 0)
    {
        process_release(process);
        return -ENOMEM;
    }

    *status = process_wait(process);
    process_release(process);
    return 0;
}

void init_start(const boot_info_t *info)
{
    int passed = 0;
    int failed = 0;

    for (size_t i = 0; i < info->module_count; i++)
    {
        const boot_module_t *module = &info->modules[i];

        // The module string is "<name> [expected exit status]" (see grub.cfg)
        size_t space = 0;
        while (module->name[space] != '\0' && module->name[space] != ' ')
            space++;

        int expected = module->name[space] == ' ' ? atoi(&module->name[space + 1]) : 0;

        // kprintf has no precision (%.*s): copy the name alone
        char name[BOOT_MODULE_NAME_MAX];
        strlcpy(name, module->name, space + 1);

        int status;
        int ret = module_run(module, &status);

        if (ret < 0)
        {
            kprintf("test %s: FAIL (could not start: %d)\n", name, ret);
            failed++;
        }
        else if (status != expected)
        {
            kprintf("test %s: FAIL (status %d, expected %d)\n", name, status, expected);
            failed++;
        }
        else
        {
            kprintf("test %s: ok\n", name);
            passed++;
        }
    }

    kprintf("tests: %d passed, %d failed\n", passed, failed);
}
