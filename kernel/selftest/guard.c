#include <selftest/guard.h>
#include <sched/spinlock.h>
#include <asm/irq_flags.h>
#include <kprintf.h>
#include <panic.h>

/**
 * Return from inside a scoped guard: the guard must still be released.
 *
 * @return Whether IRQs were enabled inside the guard (must be false).
 */
static bool guard_return_inside(void)
{
    scoped_guard(irq)
        return irq_enabled();

    return true;
}

/**
 * Hold a lock with a scope-wide guard for the whole function.
 *
 * @param held Set to whether the lock was held inside the function.
 */
static void guard_lock_and_return(spinlock_t *lock, bool *held)
{
    guard(spinlock, lock);
    *held = lock->locked;
}

void guard_selftest(void)
{
    if (!irq_enabled())
        panic("Guard self-test must start with IRQs enabled\n");

    // Scope-wide guard: off inside, restored at the end of the block
    {
        guard(irq);

        if (irq_enabled())
            panic("guard(irq): IRQs enabled inside the guard\n");
    }
    if (!irq_enabled())
        panic("guard(irq): IRQs still disabled after the scope\n");

    // Early return from a scoped guard
    if (guard_return_inside())
        panic("scoped_guard(irq): IRQs enabled inside the guard\n");
    if (!irq_enabled())
        panic("scoped_guard(irq): return skipped the release\n");

    // break leaves the guarded block and still releases
    scoped_guard(irq)
    {
        break;
    }
    if (!irq_enabled())
        panic("scoped_guard(irq): break skipped the release\n");

    // Nested: the inner release restores "disabled", it must not re-enable
    scoped_guard(irq)
    {
        {
            guard(irq);
        }
        if (irq_enabled())
            panic("guard(irq): nested release re-enabled IRQs\n");
    }
    if (!irq_enabled())
        panic("scoped_guard(irq): outer release did not re-enable IRQs\n");

    // Spinlock guard: held inside, released and IRQs back on after
    spinlock_t lock = SPINLOCK_INIT;

    scoped_guard(spinlock, &lock)
    {
        if (!lock.locked || irq_enabled())
            panic("scoped_guard(spinlock): lock not held with IRQs off\n");
    }
    if (lock.locked || !irq_enabled())
        panic("scoped_guard(spinlock): lock not released\n");

    bool held = false;
    guard_lock_and_return(&lock, &held);
    if (!held || lock.locked || !irq_enabled())
        panic("guard(spinlock): lock not held or not released\n");

    // Explicit API, and the lock can be taken again once released
    irq_flags_t flags = spinlock_irqsave(&lock);
    spinlock_irqrestore(&lock, flags);
    if (lock.locked || !irq_enabled())
        panic("spinlock_irqsave: lock not released\n");

    kprintf("Guard self-test completed successfully\n");
}
