#include <syscall.h>
#include <x86/isr.h>
#include <x86/syscall.h>
#include <x86/vectors.h>

/**
 * @brief Handles system calls invoked via the syscall interrupt.
 *
 * @param context The interrupt context containing the register state at the time of the syscall.
 */
static void syscall_handler(isr_context_t *context)
{
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
}

void syscall_init(void)
{
    isr_info_t info = {
        .type = ISR_IRQ,
        .handler = syscall_handler};

    isr_set_info(VECTOR_SYSCALL, &info);
}