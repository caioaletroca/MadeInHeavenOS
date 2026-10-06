#include <selftest/console.h>
#include <driver/console.h>
#include <driver/input.h>
#include <sched/thread.h>
#include <kprintf.h>
#include <panic.h>
#include <string.h>

// Long enough for the reader to be asleep in console_read before any key arrives
#define CONSOLE_TEST_DELAY_TICKS 5

/**
 * Press and release a key, as the keyboard IRQ would report it.
 */
static void console_test_type(keycode_t key)
{
    input_report_key(key, true);
    input_report_key(key, false);
}

/**
 * Type "hi", Enter, then Ctrl+D on the empty next line.
 */
static void console_test_feeder(void *arg)
{
    (void)arg;

    thread_sleep(CONSOLE_TEST_DELAY_TICKS);

    console_test_type(KEY_H);
    console_test_type(KEY_I);
    console_test_type(KEY_ENTER);

    input_report_key(KEY_LEFT_CTRL, true);
    console_test_type(KEY_D);
    input_report_key(KEY_LEFT_CTRL, false);
}

void console_selftest(void)
{
    char line[16];

    if (thread_create(console_test_feeder, NULL) == NULL)
        panic("Console self-test: failed to create the feeder thread\n");

    // Blocks: the feeder waits first, so this sleeps with console_lock released
    size_t length = console_read(line, sizeof(line));

    if (length != 3 || memcmp(line, "hi\n", 3) != 0)
        panic("Console self-test: read %u bytes instead of \"hi\\n\"\n", (unsigned int)length);

    // Ctrl+D on an empty line is EOF: read returns 0
    length = console_read(line, sizeof(line));

    if (length != 0)
        panic("Console self-test: EOF read returned %u bytes instead of 0\n", (unsigned int)length);

    kprintf("Console self-test completed successfully\n");
}
