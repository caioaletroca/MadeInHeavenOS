#include <driver/timer.h>
#include <platform/platform.h>
#include <sched/scheduler.h>

static volatile uint64_t ticks;
static uint64_t tick_nanoseconds;

void timer_tick(void)
{
    ticks++;
    scheduler_tick();
}

void timer_init(uint32_t frequency)
{
    tick_nanoseconds = platform_timer_init(frequency);
}

uint64_t timer_ticks(void)
{
    return ticks;
}

uint64_t timer_ticks_for(uint64_t nanoseconds)
{
    // Rounded up, plus one: the tick in progress is already partly over, and a
    // sleep must last at least what was asked
    return (nanoseconds + tick_nanoseconds - 1) / tick_nanoseconds + 1;
}

uint64_t timer_nanoseconds(void)
{
    return ticks * tick_nanoseconds;
}