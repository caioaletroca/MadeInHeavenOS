#include <stdbool.h>
#include <driver/tty.h>
#include <asm/irq_flags.h>

// Drawn for malformed UTF-8
#define UTF8_REPLACEMENT 0xFFFD

static struct screen scr;

// UTF-8 decoder state: a sequence may be split across tty_write calls
static uint32_t utf8_codepoint;     // Code point being assembled
static uint32_t utf8_minimum;       // Smallest value valid for this length (rejects overlong forms)
static unsigned int utf8_remaining; // Continuation bytes still expected

/**
 * Feed one byte of UTF-8 into the decoder, drawing every completed code point.
 */
static void tty_putchar(unsigned char byte)
{
    // Continuation byte: 10xxxxxx
    if ((byte & 0xC0) == 0x80)
    {
        // Not inside a sequence
        if (utf8_remaining == 0)
        {
            screen_put(&scr, UTF8_REPLACEMENT);
            return;
        }

        utf8_codepoint = (utf8_codepoint << 6) | (byte & 0x3F);

        if (--utf8_remaining == 0)
        {
            bool valid = utf8_codepoint >= utf8_minimum && utf8_codepoint <= 0x10FFFF && !(utf8_codepoint >= 0xD800 && utf8_codepoint <= 0xDFFF);

            screen_put(&scr, valid ? utf8_codepoint : UTF8_REPLACEMENT);
        }

        return;
    }

    // Any other byte starts a new character, so a pending sequence was cut short
    if (utf8_remaining > 0)
    {
        utf8_remaining = 0;
        screen_put(&scr, UTF8_REPLACEMENT);
    }

    if (byte < 0x80)
    {
        // 0xxxxxxx: ASCII, including control characters
        screen_put(&scr, byte);
    }
    else if ((byte & 0xE0) == 0xC0)
    {
        // 110xxxxx: one continuation byte follows
        utf8_codepoint = byte & 0x1F;
        utf8_minimum = 0x80;
        utf8_remaining = 1;
    }
    else if ((byte & 0xF0) == 0xE0)
    {
        // 1110xxxx: two continuation bytes follow
        utf8_codepoint = byte & 0x0F;
        utf8_minimum = 0x800;
        utf8_remaining = 2;
    }
    else if ((byte & 0xF8) == 0xF0)
    {
        // 11110xxx: three continuation bytes follow
        utf8_codepoint = byte & 0x07;
        utf8_minimum = 0x10000;
        utf8_remaining = 3;
    }
    else
    {
        // 0xF8..0xFF never appear in UTF-8
        screen_put(&scr, UTF8_REPLACEMENT);
    }
}

size_t tty_write(const void *str, size_t n)
{
    const unsigned char *bytes = str;

    irq_flags_t flags = irq_save();

    for (size_t i = 0; i < n; i++)
        tty_putchar(bytes[i]);

    // TODO: Adjust timers for screen update
    screen_update(&scr);

    irq_restore(flags);

    return n;
}

void tty_init()
{
    utf8_remaining = 0;
    screen_init(&scr);
}