#ifndef _DRIVER_CONSOLE_H_
#define _DRIVER_CONSOLE_H_

#include <stdint.h>
#include <stddef.h>
#include <driver/keymap.h>
#include <fs/file.h>

/**
 * Initialize the console with the specified keymap.
 *
 * @param map The keymap to use for the console.
 */
void console_init(const keymap_t *map);

/**
 * Open /dev/console, the file console_file hands out. Once, after /dev is mounted.
 */
void console_open(void);

/**
 * Set the keymap for the console.
 *
 * @param map The keymap to set for the console.
 */
void console_set_keymap(const keymap_t *map);

/**
 * Read input from the console.
 *
 * @param buf The buffer to store the read input.
 * @param n The maximum number of bytes to read.
 * @return The number of bytes actually read.
 */
size_t console_read(char *buf, size_t n);

/**
 * Write output to the console.
 *
 * @param buf The buffer containing the output to write.
 * @param n The number of bytes to write.
 * @return The number of bytes actually written.
 */
size_t console_write(const char *buf, size_t n);

/**
 * Get the console's open file: /dev/console, opened read-write once by
 * console_init. Every caller shares that one file, like fds made with dup
 * (init gets it as fds 0, 1 and 2).
 *
 * @return A new reference to the file (file_put it when done).
 */
file_t *console_file(void);

#endif // _DRIVER_CONSOLE_H_