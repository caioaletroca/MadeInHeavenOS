#include <exec/exec.h>
#include <sched/process.h>
#include <driver/console.h>
#include <mm/address_space.h>
#include <asm/memory.h>
#include <fs/file.h>
#include <fs/initrd.h>
#include <boot_info.h>
#include <syscall.h>
#include <string.h>
#include <kprintf.h>
#include <panic.h>

/*
 * Starts /sbin/init from the initrd as pid 1. Its arguments are the words of
 * the kernel command line (grub.cfg: "multiboot2 /boot/mihos.kernel once
 * /usr/tests/run"). Init never exits: if it does, the kernel panics.
 */

#define INIT_PATH "/sbin/init"
#define INIT_ARGS_MAX 8 // argv entries, INIT_PATH included

/**
 * Splits a line in place at spaces into arguments (no quotes or escapes).
 *
 * @param line The input line to split.
 * @param argv Array to store the argument pointers.
 * @param max Maximum number of arguments to store; argv needs max + 1 slots (NULL).
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
 * Run a program as pid 1 with the console on fds 0-2 and wait for it.
 *
 * @param image The program's ELF image (in place in the initrd).
 * @param size The image's size in bytes.
 * @param argv The argument vector for the program.
 * @param status Set to the exit status of the process on success.
 * @return 0 with *status set, or a negative errno if it could not be started.
 */
static int program_run(const void *image, size_t size, char *const argv[], int *status)
{
    char *const envp[] = {"PATH=/bin", NULL};

    address_space_t *space;
    uintptr_t entry;
    uintptr_t stack;
    int ret = exec_load(image, size, argv, envp, &space, &entry, &stack);
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
    size_t size;
    const void *image = initrd_find(INIT_PATH, &size);
    if (image == NULL)
        panic("init_start: no %s in the initrd\n", INIT_PATH);

    // argv = {"/sbin/init", the words of the kernel command line...}
    char line[BOOT_CMDLINE_MAX];
    char *argv[INIT_ARGS_MAX + 1];
    strlcpy(line, info->cmdline, sizeof(line));
    argv[0] = INIT_PATH;
    args_split(line, &argv[1], INIT_ARGS_MAX - 1); // slot 0 is INIT_PATH

    int status;
    int ret = program_run(image, size, argv, &status);
    if (ret < 0)
        panic("init_start: failed to start /sbin/init: %d\n", ret);

    // Init never exits: it halts the system through SYS_REBOOT
    panic("init exited with status %d\n", status);
}
