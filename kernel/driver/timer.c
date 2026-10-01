#include <driver/timer.h>
#include <platform/platform.h>
#include <sched/scheduler.h>

static volatile uint64_t ticks;

void timer_tick(void)
{
    ticks++;
    scheduler_tick();
}

void timer_init(uint32_t frequency)
{
    platform_timer_init(frequency);
}

uint64_t timer_ticks(void)
{
    return ticks;
}
