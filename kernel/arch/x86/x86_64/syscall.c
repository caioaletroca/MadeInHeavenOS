#include <syscall.h>
#include <asm/irq_flags.h>
#include <x86/isr.h>
#include <x86/syscall.h>
#include <x86/vectors.h>
#include <sched/scheduler.h>
#include <assert.h>

/**
 * @brief Handles system calls invoked via the syscall interrupt.
 *
 * @param context The interrupt context containing the register state at the time of the syscall.
 */
static void syscall_handler(isr_context_t *context)
{
    scheduler_current()->syscall_number = (long)context->rax;

    // The gate left IRQs off. The call itself runs with them on: the timer and the devices
    // get through, and a long call is preempted like a kernel thread. Shared state has its
    // own locks (a spinlock turns IRQs off itself while it is held)
    irq_enable();

    // rax = number, rdi rsi rdx r10 r8 r9 = arguments; the result goes back
    // in the saved rax, which isr_common restores before iretq
    context->rax = (uint64_t)syscall_dispatch(
        context->rax,
        context->rdi,
        context->rsi,
        context->rdx,
        context->r10,
        context->r8,
        context->r9);

    // IRQs off here means a spinlock was taken and never released: no call may return holding one
    KASSERT(irq_enabled());

    // Off again for the way out: the reschedule check, signal delivery and iretq expect it
    irq_disable();
}

void syscall_init(void)
{
    isr_info_t info = {
        .type = ISR_IRQ,
        .handler = syscall_handler};

    isr_set_info(VECTOR_SYSCALL, &info);
}