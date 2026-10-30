#ifndef _KERNEL_ARGS_H_
#define _KERNEL_ARGS_H_

#include <driver/block.h>

/**
 * The kernel's part of the command line: the leading name=value words. The
 * first word without '=' and everything after it belong to init.
 *
 * "root=hdb1 respawn /bin/sh" → root "hdb1", init_args "respawn /bin/sh"
 */
typedef struct kernel_args
{
    char root[BLOCK_NAME_MAX]; // root=: the block device holding '/'; "" = the initrd
    const char *init_args;     // init's words: points into the parsed command line
} kernel_args_t;

/**
 * @brief Split the command line into the kernel's options and init's arguments.
 *
 * Unknown names are printed and ignored. A root= value too long for a device
 * name panics. The command line must outlive args (init_args points into it).
 *
 * @param cmdline The whole command line ('\0'-terminated).
 * @param args Receives the options and where init's words start.
 */
void kernel_args_parse(const char *cmdline, kernel_args_t *args);

#endif /* _KERNEL_ARGS_H_ */