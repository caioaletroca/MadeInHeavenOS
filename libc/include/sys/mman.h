#ifndef _SYS_MMAN_H_
#define _SYS_MMAN_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <mihos/mman.h>
#include <stddef.h>

__BEGIN_DECLS

#define MAP_FAILED ((void *)-1)

/**
 * @brief Maps memory: anonymous and private only for now (MAP_PRIVATE | MAP_ANONYMOUS).
 *
 * @return The start of the mapping, or MAP_FAILED with errno set.
 */
void *mmap(void *address, size_t length, int protection, int flags, int fd, off_t offset);

/**
 * @brief Removes the mappings in [address, address + length).
 *
 * @return 0, or -1 with errno set.
 */
int munmap(void *address, size_t length);

/**
 * @brief Changes the protection of [address, address + length), which must be mapped.
 *
 * @return 0, or -1 with errno set.
 */
int mprotect(void *address, size_t length, int protection);

__END_DECLS

#endif // _SYS_MMAN_H_