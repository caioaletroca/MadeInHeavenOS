#ifndef _SCHED_FUTEX_H_
#define _SCHED_FUTEX_H_

#include <stddef.h>
#include <stdint.h>

struct address_space;

#define FUTEX_NO_TIME_LIMIT UINT64_MAX

/**
 * @brief Sleep while the 32-bit word at address (in space) holds expected. The check and
 * the queueing happen under one lock, so a wake that follows a change is never missed.
 *
 * @param ticks Time limit in timer ticks; 0 checks only; FUTEX_NO_TIME_LIMIT for none.
 * @return 0 when woken, -EAGAIN (the word differs), -ETIMEDOUT, -EINTR (a signal),
 *         -EINVAL (not 4-byte aligned), -EFAULT (not readable).
 */
int futex_wait(struct address_space *space, uintptr_t address, uint32_t expected, uint64_t ticks);

/**
 * @brief Wake up to count threads sleeping on the word at address (in space).
 *
 * @return How many were woken.
 */
size_t futex_wake(struct address_space *space, uintptr_t address, size_t count);

#endif // _SCHED_FUTEX_H_