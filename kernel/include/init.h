#ifndef _INIT_H_
#define _INIT_H_

#include <boot_info.h>

/**
 * @brief Start the first user program from the boot modules.
 *
 * @param boot_info Pointer to the boot information structure.
 */
void init_start(const boot_info_t *boot_info);

#endif