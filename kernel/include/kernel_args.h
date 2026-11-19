#ifndef _KERNEL_ARGS_H_
#define _KERNEL_ARGS_H_

#include <stdbool.h>
#include <driver/block.h>
#include <logger.h>

/**
 * The kernel's part of the command line: the leading words that are name=value,
 * or the bare name of a switch (selftests alone means selftests=1). The first
 * other bare word, and everything after it, belong to init; "--" ends the
 * kernel's words explicitly.
 *
 * "root=hdb1 respawn /bin/sh"       → root "hdb1", init_args "respawn /bin/sh"
 * "selftests once /usr/tests/run"   → selftests on, init_args "once /usr/tests/run"
 * "-- selftests"                    → init_args "selftests"
 */
typedef struct kernel_args
{
    char root[BLOCK_NAME_MAX]; // root=: the block device holding '/'; "" = the initrd
    bool selftests;            // selftests=1: run the kernel's self-tests before init (GRUB's test entries)
    int console_level;         // quiet (4) or loglevel=N: messages below it reach the console
    const char *init_args;     // init's words: points into the parsed command line
} kernel_args_t;

/**
 * @brief Split the command line into the kernel's options and init's arguments.
 *
 * Unknown names, values a parameter cannot take, and a missing value for a
 * parameter that is not a switch are printed and ignored; the rest of the line
 * is still parsed. A root= value too long for a device name panics. The
 * command line must outlive args (init_args points into it).
 *
 * @param cmdline The whole command line ('\0'-terminated).
 * @param args Receives the options and where init's words start.
 */
void kernel_args_parse(const char *cmdline, kernel_args_t *args);

#endif /* _KERNEL_ARGS_H_ */