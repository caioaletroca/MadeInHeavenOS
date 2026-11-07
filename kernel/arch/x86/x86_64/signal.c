#include <arch/signal.h>
#include <asm/memory.h>
#include <mm/address_space.h>
#include <x86/cpu.h>
#include <x86/isr.h>
#include <x86/vectors.h>
#include <sched/thread.h>
#include <mihos/errno.h>
#include <util.h>

#define RED_ZONE_SIZE 128

// CF, PF, AF, ZF, SF, DF, OF: the flags a program may set. Never IOPL or IF
#define RFLAGS_USER_RESTORED 0xCD5ULL

#define SYSCALL_INSTRUCTION_SIZE 2 // int $0x80

typedef struct signal_frame
{
    uint64_t return_address;
    uint64_t blocked;
    isr_context_t registers;
} signal_frame_t;

/**
 * @brief Returns a pointer to the frame of the trap from ring 3, at the top of the kernel stack.
 *
 * @param stack The lowest address of the thread's kernel stack.
 * @param stack_size The size of the kernel stack.
 * @return A pointer to the isr_context_t at the top of the stack.
 */
static isr_context_t *user_frame(void *stack, size_t stack_size)
{
    uintptr_t top = ALIGN_DOWN((uintptr_t)stack + stack_size, ABI_STACK_ALIGNMENT);
    return (isr_context_t *)top - 1;
}

/**
 * @brief Checks if the system call was interrupted by a signal.
 *
 * @param context The interrupt context of the system call.
 * @return true if the system call was interrupted, false otherwise.
 */
static bool syscall_interrupted(isr_context_t *context)
{
    uint8_t vector = (uint8_t)(context->info >> 32) & 0xFF;
    return vector == VECTOR_SYSCALL && (long)context->rax == -EINTR;
}

void arch_syscall_restart(void *stack, size_t stack_size, long number)
{
    isr_context_t *context = user_frame(stack, stack_size);

    if (number != SYSCALL_NOT_RESTARTABLE && syscall_interrupted(context))
    {
        context->rax = (uint64_t)number;
        context->rip -= SYSCALL_INSTRUCTION_SIZE;
    }
}

int arch_signal_frame_push(void *stack, size_t stack_size, struct address_space *space, int signal,
                           uintptr_t handler, uintptr_t restorer, uint32_t blocked)
{
    isr_context_t *context = user_frame(stack, stack_size);

    // SysV ABI at a function's entry: rsp + 8 is 16-aligned, as `call` leaves it
    uintptr_t address = context->rsp - RED_ZONE_SIZE - sizeof(signal_frame_t);
    address = ALIGN_DOWN(address, ABI_STACK_ALIGNMENT) - sizeof(uint64_t);

    signal_frame_t frame = {
        .return_address = restorer,
        .blocked = blocked,
        .registers = *context,
    };

    if (address_space_write(space, address, &frame, sizeof(frame)) != 0)
        return -1;

    context->rip = handler;
    context->rsp = address;
    context->rdi = (uint64_t)signal;
    context->rflags &= ~RFLAGS_DF;

    return 0;
}

int arch_signal_frame_pop(void *stack, size_t stack_size, struct address_space *space,
                          uint32_t *blocked, long *result)
{
    isr_context_t *context = user_frame(stack, stack_size);
    signal_frame_t frame;

    if (address_space_read(space, context->rsp - sizeof(uint64_t), &frame, sizeof(frame)) != 0)
        return -1;

    if (frame.registers.rip >= USER_TOP || frame.registers.rsp >= USER_TOP)
        return -1;

    frame.registers.cs = context->cs;
    frame.registers.ss = context->ss;
    frame.registers.info = context->info;
    frame.registers.rflags = (frame.registers.rflags & RFLAGS_USER_RESTORED) | RFLAGS_USER_THREAD;

    *context = frame.registers;
    *blocked = (uint32_t)frame.blocked;
    *result = (long)frame.registers.rax;

    return 0;
}