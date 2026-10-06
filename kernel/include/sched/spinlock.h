#ifndef _SCHED_SPINLOCK_H
#define _SCHED_SPINLOCK_H

#include <stdbool.h>
#include <asm/irq_flags.h>
#include <cleanup.h>
#include <panic.h>

/*
 * Single CPU: disabling IRQs is what makes a critical section exclusive, so
 * every lock here disables them. `locked` only catches misuse (recursion,
 * double unlock); on SMP it becomes the atomic lock word behind the same API.
 */

/**
 * @brief IRQ guard: `guard(irq);` disables IRQs until the end of the scope.
 */
typedef struct
{
    irq_flags_t flags;
} irq_guard_t;

/**
 * @brief Disable IRQs, saving the previous state (`guard(irq)` acquire).
 */
static inline irq_guard_t irq_guard_init(void)
{
    return (irq_guard_t){.flags = irq_save()};
}

/**
 * @brief Restore the IRQ state saved by `irq_guard_init` (`guard(irq)` release).
 */
static inline void irq_guard_exit(irq_guard_t *guard)
{
    irq_restore(guard->flags);
}

/**
 * @brief Spinlock. Taking it always disables IRQs (see above).
 */
typedef struct
{
    bool locked;
} spinlock_t;

/**
 * @brief Static initializer for a spinlock.
 *
 * This macro provides a convenient way to initialize a spinlock
 * to its unlocked state at compile time.
 */
#define SPINLOCK_INIT {.locked = false}

/**
 * @brief Initialize a spinlock.
 *
 * This function sets the spinlock to its initial unlocked state.
 */
static inline void spinlock_init(spinlock_t *lock)
{
    lock->locked = false;
}

/**
 * @brief Acquire the spinlock. Caller must ensure that IRQs are disabled.
 *
 * Panics if the lock is already held.
 *
 * @param lock Pointer to the spinlock to acquire.
 */
static inline void spinlock_acquire(spinlock_t *lock)
{
    if (lock->locked)
        panic("spinlock_acquire: lock %p already held\n", (void *)lock);

    if (irq_enabled())
        panic("spinlock_acquire: IRQs are enabled while acquiring lock %p\n", (void *)lock);

    lock->locked = true;
}

/**
 * @brief Release the spinlock.
 *
 * Panics if the lock is not held.
 *
 * @param lock Pointer to the spinlock to release.
 */
static inline void spinlock_release(spinlock_t *lock)
{
    if (!lock->locked)
        panic("spinlock_release: lock %p not held\n", (void *)lock);

    lock->locked = false;
}

/**
 * @brief Disable IRQs and take the lock.
 *
 * IRQs go off first, so the check cannot be preempted. On one CPU there is
 * nothing to spin on: a held lock means this CPU already holds it (the same
 * thread again, or an IRQ handler taking its own thread's lock), which would
 * deadlock on SMP, so it panics.
 *
 * @param lock Pointer to the spinlock to acquire.
 * @return The previous IRQ state, for `spinlock_irqrestore`.
 */
static inline irq_flags_t spinlock_irqsave(spinlock_t *lock)
{
    irq_flags_t flags = irq_save();
    spinlock_acquire(lock);
    return flags;
}

/**
 * @brief Release the lock, then restore the IRQ state.
 *
 * The lock is released while IRQs are still off; panics on a double unlock.
 *
 * @param lock Pointer to the spinlock to release.
 * @param flags The IRQ state returned by `spinlock_irqsave`.
 */
static inline void spinlock_irqrestore(spinlock_t *lock, irq_flags_t flags)
{
    spinlock_release(lock);
    irq_restore(flags);
}

/**
 * @brief Spinlock guard: `guard(spinlock, &lock);` holds the lock (IRQs off)
 * until the end of the scope.
 */
typedef struct
{
    spinlock_t *lock;
    irq_flags_t flags;
} spinlock_guard_t;

/**
 * @brief Take the lock (`guard(spinlock, lock)` acquire).
 *
 * @param lock Pointer to the spinlock to acquire.
 * @return The guard state: the lock and the previous IRQ state.
 */
static inline spinlock_guard_t spinlock_guard_init(spinlock_t *lock)
{
    spinlock_guard_t guard = {
        .lock = lock,
        .flags = spinlock_irqsave(lock)};
    return guard;
}

/**
 * @brief Release the lock and restore the IRQ state (`guard(spinlock, lock)` release).
 *
 * @param guard Pointer to the guard state.
 */
static inline void spinlock_guard_exit(spinlock_guard_t *guard)
{
    spinlock_irqrestore(guard->lock, guard->flags);
}

#endif // _SCHED_SPINLOCK_H