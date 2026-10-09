#ifndef _INIT_H_
#define _INIT_H_

#include <boot_info.h>

/**
 * @brief Start /sbin/init from the boot modules, with its module string as argv.
 *
 * Temporary: init exiting means power off (prints its status and returns);
 * once there is a shell, init never exits and this becomes a panic.
 *
 * @param boot_info Pointer to the boot information structure.
 */
void init_start(const boot_info_t *boot_info);

#endif