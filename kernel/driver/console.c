#include <driver/console.h>
#include <sched/thread.h>
#include <driver/input.h>
#include <driver/tty.h>
#include <sched/wait.h>
#include <asm/irq_flags.h>
#include <stdbool.h>
#include <panic.h>

#define CONSOLE_BUFFER_SIZE 1024
_Static_assert((CONSOLE_BUFFER_SIZE & (CONSOLE_BUFFER_SIZE - 1)) == 0, "CONSOLE_BUFFER_SIZE must be a power of two");

#define CTRL(c) ((c) & 0x1F)

static char buffer[CONSOLE_BUFFER_SIZE];
static unsigned int read_index;
static unsigned int write_index;
static unsigned int edit_index;

static wait_queue_t readers;

static const keymap_t *keymap;
static bool down[KEY_COUNT];
static bool caps_lock;
static bool num_lock = true;
static uint32_t pending_accent;

/**
 * Encode a code point as UTF-8.
 *
 * @return Number of bytes written to out.
 */
static size_t utf8_encode(uint32_t c, char out[4])
{
    if (c < 0x80)
    {
        out[0] = c;
        return 1;
    }

    if (c < 0x800)
    {
        out[0] = 0xC0 | (c >> 6);
        out[1] = 0x80 | (c & 0x3F);
        return 2;
    }

    if (c < 0x10000)
    {
        out[0] = 0xE0 | (c >> 12);
        out[1] = 0x80 | ((c >> 6) & 0x3F);
        out[2] = 0x80 | (c & 0x3F);
        return 3;
    }

    out[0] = 0xF0 | (c >> 18);
    out[1] = 0x80 | ((c >> 12) & 0x3F);
    out[2] = 0x80 | ((c >> 6) & 0x3F);
    out[3] = 0x80 | (c & 0x3F);
    return 4;
}

/**
 * Append bytes to the line being edited, all or nothing so a character is never split.
 *
 * Ordinary input leaves one byte free, so a '\n' or EOF can always end the
 * line; otherwise a full buffer would never commit and readers would wait forever.
 */
static bool console_store(const char *bytes, size_t n, bool ends_line)
{
    irq_flags_t flags = irq_save();
    size_t used = edit_index - read_index;
    size_t limit = ends_line ? CONSOLE_BUFFER_SIZE : CONSOLE_BUFFER_SIZE - 1;
    bool fits = used + n <= limit;

    if (fits)
        for (size_t i = 0; i < n; i++)
            buffer[edit_index++ % (CONSOLE_BUFFER_SIZE - 1)] = bytes[i];

    irq_restore(flags);

    return fits;
}

/**
 * Hand the edited line to readers.
 */
static void console_commit(void)
{
    irq_flags_t flags = irq_save();
    write_index = edit_index;
    wait_queue_wake_all(&readers);
    irq_restore(flags);
}

/**
 * Erase the last character of the line being edited.
 *
 * @return false if the line was already empty.
 */
static bool console_erase(void)
{
    if (edit_index == write_index)
        return false;

    do
        edit_index--;
    while (edit_index != write_index && ((unsigned char)buffer[edit_index % CONSOLE_BUFFER_SIZE] & 0xC0) == 0x80);

    tty_write("\b \b", 3);
    return true;
}

/**
 * Handle a code point input from the keyboard.
 *
 * @param codepoint The Unicode code point received from the keyboard.
 */
static void console_input(uint32_t codepoint)
{
    char bytes[4];

    switch (codepoint)
    {
    case '\b':
    case 0x7F:
        console_erase();
        return;
    case CTRL('U'):
        while (console_erase())
            ;
        return;
    case CTRL('C'):
        edit_index = write_index;
        tty_write("^C\n", 3);
        return;
    case '\n':
        if (console_store("\n", 1, true))
        {
            tty_write("\n", 1);
            console_commit();
        }
        return;
    case CTRL('D'):
        bytes[0] = CTRL('D');
        if (console_store(bytes, 1, true))
            console_commit();
        return;
    }

    if (codepoint < 0x20)
        return;

    size_t n = utf8_encode(codepoint, bytes);

    if (console_store(bytes, n, false))
        tty_write(bytes, n);
}

/**
 * Get the current state of modifier keys.
 *
 * @return A bitmask representing the active modifier keys.
 */
static unsigned int console_modifiers(void)
{
    unsigned int modifiers = 0;

    if (down[KEY_LEFT_SHIFT] || down[KEY_RIGHT_SHIFT])
        modifiers |= KEYMAP_SHIFT;
    if (down[KEY_RIGHT_ALT])
        modifiers |= KEYMAP_ALTGR;
    if (caps_lock)
        modifiers |= KEYMAP_CAPS_LOCK;
    if (num_lock)
        modifiers |= KEYMAP_NUM_LOCK;

    return modifiers;
}

static void console_key(input_event_t event)
{
    if (event.key <= KEY_NONE || event.key >= KEY_COUNT)
        return;

    bool repeat = event.pressed && down[event.key];
    down[event.key] = event.pressed;

    if (!event.pressed)
        return;

    if (!repeat && event.key == KEY_CAPS_LOCK)
        caps_lock = !caps_lock;
    if (!repeat && event.key == KEY_NUM_LOCK)
        num_lock = !num_lock;

    uint32_t c = keymap_translate(keymap, event.key, console_modifiers());
    if (c == 0)
        return;

    if (c & KEYMAP_DEAD)
    {
        uint32_t accent = c & ~KEYMAP_DEAD;

        if (pending_accent != 0)
        {
            uint32_t previous = pending_accent;
            pending_accent = 0;
            console_input(previous);

            if (previous == accent)
                return;
        }

        pending_accent = accent;
        return;
    }

    // Before composing, so Ctrl+C with a pending accent still cancels
    if ((down[KEY_LEFT_CTRL] || down[KEY_RIGHT_CTRL]) && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
        c = CTRL(c);

    if (pending_accent != 0)
    {
        uint32_t accent = pending_accent;
        pending_accent = 0;

        if (c == ' ')
            c = accent;
        else if (c >= 0x20 && c != 0x7F)
        {
            uint32_t composed = keymap_compose(accent, c);

            if (composed != 0)
                c = composed;
            else
                console_input(accent);
        }
    }

    console_input(c);
}

static void console_thread(void *arg)
{
    (void)arg;

    for (;;)
        console_key(input_get_event());
}

void console_init(const keymap_t *map)
{
    keymap = map;
    wait_queue_init(&readers);

    if (thread_create(console_thread, NULL) == NULL)
        panic("console: failed to create the console thread\n");
}

void console_set_keymap(const keymap_t *map)
{
    keymap = map;
}

size_t console_read(char *buf, size_t n)
{
    size_t count = 0;

    if (n == 0)
        return 0;

    irq_flags_t flags = irq_save();

    while (read_index == write_index)
        wait_queue_sleep(&readers);

    while (count < n && read_index != write_index)
    {
        char c = buffer[read_index % CONSOLE_BUFFER_SIZE];

        if (c == CTRL('C'))
        {
            // Consume EOF only when it comes first; otherwise the next read returns 0
            if (count == 0)
                read_index++;
            break;
        }

        read_index++;
        buf[count++] = c;

        // Canonical mode: one line per read
        if (c == '\n')
            break;
    }

    irq_restore(flags);
    return count;
}

size_t console_write(const char *buf, size_t n)
{
    return tty_write(buf, n);
}