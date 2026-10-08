#ifndef _INIT_H_
#define _INIT_H_

#include <boot_info.h>

/**
 * @brief Run every boot module as a test program and report each exit status.
 *
 * Temporary: becomes "start /sbin/init" once spawn and wait syscalls exist.
 *
 * @param boot_info Pointer to the boot information structure.
 */
void init_start(const boot_info_t *boot_info);

#endif