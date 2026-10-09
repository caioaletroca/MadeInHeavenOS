#include <exec/exec.h>
#include <sched/process.h>
#include <driver/console.h>
#include <mm/address_space.h>
#include <asm/memory.h>
#include <fs/file.h>
#include <boot_info.h>
#include <syscall.h>
#include <string.h>
#include <kprintf.h>
#include <panic.h>

/*
 * TEMPORARY test runner: runs every boot module in grub.cfg order and checks
 * its exit status. Replaced by starting /sbin/init alone once spawn and wait
 * syscalls exist; running the tests then becomes a user-space job.
 */

#define INIT_ARGS_MAX 8

/**
 * Splits a line into arguments, similar to how a shell would.
 *
 * @param line The input line to split.
 * @param argv Array to store the argument pointers.
 * @param max Maximum number of arguments to store.
 * @return The number of arguments found.
 */
static size_t args_split(char *line, char *argv[], size_t max)
{
    size_t argc = 0;
    char *p = line;

    while (*p != '\0' && argc < max)
    {
        while (*p == ' ')
            p++;
        if (*p == '\0')
            break;

        argv[argc++] = p;
        while (*p != '\0' && *p != ' ')
            p++;
        if (*p == ' ')
            *p++ = '\0';
    }

    argv[argc] = NULL;
    return argc;
}

/**
 * Run a boot module as a process with the console on fds 0-2 and wait for it.
 *
 * @param module The boot module to run as a process.
 * @param name The program's name without arguments: its argv[0].
 * @param status Set to the exit status of the process on success.
 * @return 0 with *status set, or a negative errno if it could not be started.
 */
static int module_run(const boot_module_t *module, char *const argv[], int *status)
{
    char *const envp[] = {"PATH=/bin", NULL};

    address_space_t *space;
    uintptr_t entry;
    uintptr_t stack;
    int ret = exec_load(phys_to_kern(module->start), module->end - module->start, argv, envp, &space, &entry,
                        &stack);
    if (ret < 0)
        return ret;

    // From here the process owns the space: releasing it destroys both
    process_t *process = process_create_init(space);
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
    if (process_start(process, entry, stack) < 0)
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
    // Programs spawn others by path: the boot modules stand in for files until there is a VFS
    exec_modules_init(info);

    const boot_module_t *module = exec_module_find("/sbin/init");
    if (module == NULL)
        panic("init_start: no /sbin/init module\n");

    char line[BOOT_MODULE_NAME_MAX];
    char *argv[INIT_ARGS_MAX + 1];
    strlcpy(line, module->name, sizeof(line));
    args_split(line, argv, INIT_ARGS_MAX);

    int status;
    int ret = module_run(module, argv, &status);
    if (ret < 0)
        panic("init_start: failed to start /sbin/init: %d\n", ret);

    // TODO: init must never exit; until there is a shell, its exit means power off
    kprintf("init exited with status %d: system halted\n", status);
}
