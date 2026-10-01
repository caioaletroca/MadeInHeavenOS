#include <arch/arch.h>
#include <asm/paging.h>
#include <sched/scheduler.h>
#include <x86/exceptions.h>
#include <x86/idt.h>
#include <x86/isr.h>
#include <x86/vectors.h>

static void scheduler_yield_handler(isr_context_t *context)
{
    (void)context;
    scheduler_request_reschedule();
}

void arch_init(void)
{
    // Create and fill the IDT
    idt_init();

    // CPU exceptions, then the page fault handler on top of them
    exception_init();
    paging_init();

    // Software interrupt used by arch_yield()
    isr_info_t yield_info = {
        .type = ISR_IRQ,
        .handler = scheduler_yield_handler,
    };
    isr_set_info(VECTOR_SCHEDULER_YIELD, &yield_info);

    idt_load();
}
