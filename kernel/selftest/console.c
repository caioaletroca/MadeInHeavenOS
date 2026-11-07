#include <selftest/console.h>
#include <driver/console.h>
#include <driver/input.h>
#include <mihos/ioctl.h>
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

/**
 * The keys with no character, for a program in raw mode.
 */
static void console_test_raw_feeder(void *arg)
{
    (void)arg;

    thread_sleep(CONSOLE_TEST_DELAY_TICKS);

    console_test_type(KEY_UP);
    console_test_type(KEY_DOWN);
    console_test_type(KEY_RIGHT);
    console_test_type(KEY_LEFT);
    console_test_type(KEY_HOME);
    console_test_type(KEY_END);
    console_test_type(KEY_DELETE);
}

/**
 * The same kind of key on a line: it adds nothing to it.
 */
static void console_test_line_feeder(void *arg)
{
    (void)arg;

    thread_sleep(CONSOLE_TEST_DELAY_TICKS);

    console_test_type(KEY_UP);
    console_test_type(KEY_H);
    console_test_type(KEY_DELETE);
    console_test_type(KEY_ENTER);
}

/**
 * Set the console's settings, as tcsetattr would.
 */
static void console_test_settings(unsigned long request, struct termios *settings)
{
    file_t *file = console_file();
    long result = file->ops->ioctl(file, request, settings);
    file_put(file);

    if (result != 0)
        panic("Console self-test: the settings request failed\n");
}

/**
 * Keys with no character reach a raw program as escape sequences. TIOCSTI has
 * no byte for them, so this is the one place that can press them.
 */
static void console_selftest_keys(void)
{
    static const char sequences[] = "\x1b[A\x1b[B\x1b[C\x1b[D\x1b[H\x1b[F\x1b[3~";
    const size_t length = sizeof(sequences) - 1;
    struct termios original;
    struct termios raw;
    char bytes[32];

    console_test_settings(MIHOS_TCGETS, &original);

    // VMIN makes the read sleep for the whole set
    raw = original;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = length;
    console_test_settings(MIHOS_TCSETS, &raw);

    if (thread_create(console_test_raw_feeder, NULL) == NULL)
        panic("Console self-test: failed to create the feeder thread\n");

    long read_length = console_read(bytes, sizeof(bytes));

    if (read_length != (long)length || memcmp(bytes, sequences, length) != 0)
        panic("Console self-test: raw keys read %u bytes\n", (unsigned int)read_length);

    console_test_settings(MIHOS_TCSETS, &original);

    if (thread_create(console_test_line_feeder, NULL) == NULL)
        panic("Console self-test: failed to create the feeder thread\n");

    read_length = console_read(bytes, sizeof(bytes));

    if (read_length != 2 || memcmp(bytes, "h\n", 2) != 0)
        panic("Console self-test: a line read %u bytes instead of \"h\\n\"\n", (unsigned int)read_length);
}

void console_selftest(void)
{
    char line[16];

    if (thread_create(console_test_feeder, NULL) == NULL)
        panic("Console self-test: failed to create the feeder thread\n");

    // Blocks: the feeder waits first, so this sleeps with console_lock released
    long length = console_read(line, sizeof(line));

    if (length != 3 || memcmp(line, "hi\n", 3) != 0)
        panic("Console self-test: read %u bytes instead of \"hi\\n\"\n", (unsigned int)length);

    // Ctrl+D on an empty line is EOF: read returns 0
    length = console_read(line, sizeof(line));

    if (length != 0)
        panic("Console self-test: EOF read returned %u bytes instead of 0\n", (unsigned int)length);

    console_selftest_keys();
}
