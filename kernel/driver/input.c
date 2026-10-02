#include <driver/input.h>
#include <sched/sync.h>
#include <asm/irq_flags.h>

#define INPUT_BUFFER_SIZE 128

input_event_t buffer[INPUT_BUFFER_SIZE];

static unsigned int head = 0; // Next slot to write (producer: keyboard IRQ)
static unsigned int tail = 0; // Next slot to read (consumer: thread)

// Semaphore to track the number of available input events in the buffer.
static semaphore_t available;

void input_init(void)
{
    head = 0;
    tail = 0;
    semaphore_init(&available, 0);
}

void input_report_key(keycode_t key, bool pressed)
{
    irq_flags_t flags = irq_save();
    unsigned int next = (head + 1) % INPUT_BUFFER_SIZE;

    // One slot stays empty to tell "full" from "empty"; drop input when full
    if (next != tail)
    {
        buffer[head].key = key;
        buffer[head].pressed = pressed;
        head = next;
        semaphore_up(&available);
    }

    irq_restore(flags);
}

input_event_t input_get_event(void)
{
    semaphore_down(&available);

    irq_flags_t flags = irq_save();

    input_event_t event = buffer[tail];
    tail = (tail + 1) % INPUT_BUFFER_SIZE;

    irq_restore(flags);

    return event;
}