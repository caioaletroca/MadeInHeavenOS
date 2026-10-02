#ifndef _MM_H_
#define _MM_H_

#include <boot_info.h>

/**
 * Initializes the memory management subsystem.
 *
 * @param info Boot information structure.
 */
void mm_init(const boot_info_t *info);

#endif // _MM_H_