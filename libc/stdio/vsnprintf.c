#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/*
 * vsnprintf with C99 semantics: every character goes through output_char,
 * which writes only while there is room and counts everything. So the output
 * is always terminated (when size > 0), never overruns, and the return value
 * is the full length: >= size means the output was cut.
 *
 * Supported: flags - 0 + space, width (digits or *), precision (.digits or .*),
 * lengths hh h l ll z, conversions d i u x X o p c s %.
 */

typedef enum
{
    LENGTH_NONE,      // int / unsigned (hh and h too: those arrive as int)
    LENGTH_LONG,      // l
    LENGTH_LONG_LONG, // ll
    LENGTH_SIZE,      // z: size_t (and its signed counterpart for d/i)
} length_t;

/**
 * @brief Structure representing the output buffer for vsnprintf.
 */
typedef struct output
{
    char *buffer;
    size_t size;   // bytes available, terminator included
    size_t length; // characters produced so far, written or not
} output_t;

/**
 * @brief Structure representing a format specifier for vsnprintf.
 */
typedef struct spec
{
    int left;        // '-': pad on the right
    int zero;        // '0': pad numbers with zeros
    int plus;        // '+': always print a sign
    int space;       // ' ': a space where a '+' would go
    int width;       // minimum field width, 0 if none
    int precision;   // -1 if none
    length_t length; // LENGTH_NONE, LENGTH_LONG, LENGTH_LONG_LONG, LENGTH_SIZE (an enum)
} spec_t;

/**
 * @brief Outputs a single character to the output buffer.
 *
 * @param out Pointer to the output buffer structure.
 * @param c Character to output.
 */
static void output_char(output_t *out, char c)
{
    if (out->length + 1 < out->size)
        out->buffer[out->length] = c;

    out->length++;
}

/**
 * @brief Outputs a repeated character to the output buffer.
 *
 * @param out Pointer to the output buffer structure.
 * @param c Character to repeat.
 * @param count Number of times to repeat the character.
 */
static void output_repeat(output_t *out, char c, int count)
{
    for (int i = 0; i < count; i++)
        output_char(out, c);
}

/**
 * @brief Outputs a string with padding according to the format specifier.
 *
 * @param out Pointer to the output buffer structure.
 * @param spec Pointer to the format specifier structure.
 * @param s String to output.
 * @param count Number of characters to output from the string.
 */
static void output_padded(output_t *out, const spec_t *spec, const char *s, size_t count)
{
    int padding = spec->width > (int)count ? spec->width - (int)count : 0;

    if (!spec->left)
        output_repeat(out, ' ', padding);

    for (size_t i = 0; i < count; i++)
        output_char(out, s[i]);

    if (spec->left)
        output_repeat(out, ' ', padding);
}

/**
 * @brief Outputs a string according to the format specifier.
 *
 * @param out Pointer to the output buffer structure.
 * @param spec Pointer to the format specifier structure.
 * @param s String to output.
 */
static void output_string(output_t *out, const spec_t *spec, const char *s)
{
    if (s == NULL)
        s = "(null)";

    size_t count = 0;
    while ((spec->precision < 0 || count < (size_t)spec->precision) && s[count] != '\0')
        count++;

    output_padded(out, spec, s, count);
}

/**
 * @brief Outputs a number according to the format specifier.
 *
 * @param out Pointer to the output buffer structure.
 * @param spec Pointer to the format specifier structure.
 * @param value Number to output.
 * @param base Numerical base for the output.
 * @param uppercase Whether to use uppercase letters for hexadecimal.
 * @param sign Sign character to output if any.
 */
static void output_number(output_t *out, const spec_t *spec, unsigned long long value, int base, int uppercase, char sign)
{
    const char *symbols = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    char digits[24]; // 22 octal digits for 64 bits, the most of any base
    int count = 0;

    // Precision 0 with value 0 prints no digits at all (C11 7.21.6.1)
    if (value != 0 || spec->precision != 0)
    {
        // Least significant first: output reverses them
        do
        {
            digits[count++] = symbols[value % base];
            value /= base;
        } while (value != 0);
    }

    int zeros = spec->precision > count ? spec->precision - count : 0;
    int total = (sign != 0) + zeros + count;
    int padding = spec->width > total ? spec->width - total : 0;

    // '0' pads with zeros after the sign, unless left-aligned or a precision is given
    if (spec->zero && !spec->left && spec->precision < 0)
    {
        zeros += padding; // add the padding to the zeros
        padding = 0;
    }

    if (!spec->left)
        output_repeat(out, ' ', padding);
    if (sign != 0)
        output_char(out, sign);

    output_repeat(out, '0', zeros);

    while (count > 0)
        output_char(out, digits[--count]);

    if (spec->left)
        output_repeat(out, ' ', padding);
}

/**
 * @brief Parses a format specifier from the format string.
 *
 * @param f Pointer to the current position in the format string.
 * @param spec Pointer to the format specifier structure to fill.
 * @param args Variable argument list for '*' width and precision (advanced past what they use).
 * @return Pointer to the conversion character (d, s, ...), not past it.
 */
static const char *spec_parse(const char *f, spec_t *spec, va_list *args)
{
    *spec = (spec_t){.precision = -1};

    for (;; f++)
    {
        if (*f == '-')
            spec->left = 1;
        else if (*f == '0')
            spec->zero = 1;
        else if (*f == '+')
            spec->plus = 1;
        else if (*f == ' ')
            spec->space = 1;
        else
            break;
    }

    if (*f == '*')
    {
        int width = va_arg(*args, int);
        if (width < 0)
        {
            spec->left = 1;
            width = -width;
        }
        spec->width = width;
        f++;
    }
    else
    {
        while (*f >= '0' && *f <= '9')
        {
            spec->width = spec->width * 10 + (*f++ - '0');
        }
    }

    if (*f == '.')
    {
        f++;
        spec->precision = 0;
        if (*f == '*')
        {
            int precision = va_arg(*args, int);
            spec->precision = precision < 0 ? -1 : precision;
            f++;
        }
        else
        {
            while (*f >= '0' && *f <= '9')
            {
                spec->precision = spec->precision * 10 + (*f++ - '0');
            }
        }
    }

    if (f[0] == 'h')
    {
        f += f[1] == 'h' ? 2 : 1;
    }
    else if (f[0] == 'l' && f[1] == 'l')
    {
        spec->length = LENGTH_LONG_LONG;
        f += 2;
    }
    else if (f[0] == 'l')
    {
        spec->length = LENGTH_LONG;
        f++;
    }
    else if (f[0] == 'z')
    {
        spec->length = LENGTH_SIZE;
        f++;
    }

    return f;
}

/**
 * @brief Retrieves the next signed argument from the variable argument list based on the length specifier.
 *
 * @param length The length specifier indicating the type of the argument.
 * @param args Pointer to the variable argument list.
 * @return The next signed argument as a long long.
 */
static long long argument_signed(length_t length, va_list *args)
{
    switch (length)
    {
    case LENGTH_LONG_LONG:
        return va_arg(*args, long long);
    case LENGTH_LONG:
        return va_arg(*args, long);
    case LENGTH_SIZE:
        return va_arg(*args, long); // the signed type of size_t's width (ssize_t)
    default:
        return va_arg(*args, int);
    }
}

/**
 * @brief Retrieves the next unsigned argument from the variable argument list based on the length specifier.
 *
 * @param length The length specifier indicating the type of the argument.
 * @param args Pointer to the variable argument list.
 * @return The next unsigned argument as an unsigned long long.
 */
static unsigned long long argument_unsigned(length_t length, va_list *args)
{
    switch (length)
    {
    case LENGTH_LONG_LONG:
        return va_arg(*args, unsigned long long);
    case LENGTH_LONG:
        return va_arg(*args, unsigned long);
    case LENGTH_SIZE:
        return va_arg(*args, size_t);
    default:
        return va_arg(*args, unsigned int);
    }
}

int vsnprintf(char *str, size_t size, const char *format, va_list args)
{
    output_t out = {
        .buffer = str,
        .size = size,
        .length = 0};

    // The helpers need a va_list they can advance: a parameter's address is not
    // a va_list * on x86-64 (va_list is an array type there), a local copy's is
    va_list ap;
    va_copy(ap, args);

    for (const char *f = format; *f != '\0'; f++)
    {
        if (*f != '%')
        {
            output_char(&out, *f);
            continue;
        }

        const char *start = f;
        spec_t spec;
        f = spec_parse(f + 1, &spec, &ap);

        switch (*f)
        {
        case 'd':
        case 'i':
        {
            long long value = argument_signed(spec.length, &ap);
            char sign = value < 0 ? '-' : spec.plus ? '+'
                                      : spec.space  ? ' '
                                                    : 0;

            unsigned long long magnitude = value < 0 ? 0ULL - (unsigned long long)value : (unsigned long long)value;
            output_number(&out, &spec, magnitude, 10, 0, sign);
            break;
        }
        case 'u':
            output_number(&out, &spec, argument_unsigned(spec.length, &ap), 10, 0, 0);
            break;
        case 'x':
            output_number(&out, &spec, argument_unsigned(spec.length, &ap), 16, 0, 0);
            break;
        case 'X':
            output_number(&out, &spec, argument_unsigned(spec.length, &ap), 16, 1, 0);
            break;
        case 'o':
            output_number(&out, &spec, argument_unsigned(spec.length, &ap), 8, 0, 0);
            break;
        case 'p':
        {
            spec_t pointer = {.zero = 1, .width = 2 * sizeof(void *), .precision = -1};
            output_number(&out, &pointer, (uintptr_t)va_arg(ap, void *), 16, 0, 0);
            break;
        }
        case 'c':
            char c = (char)va_arg(ap, int);
            output_padded(&out, &spec, &c, 1);
            break;
        case 's':
            output_string(&out, &spec, va_arg(ap, const char *));
            break;
        case '%':
            output_char(&out, '%');
            break;
        case '\0':
            // '%' at the very end: print what was there and stop; f-- lets the
            // loop's f++ land on the terminator instead of past it
            while (start < f)
                output_char(&out, *start++);
            f--;
            break;
        default:
            // Unknown conversion: print it as written, so the mistake is visible
            while (start <= f)
                output_char(&out, *start++);
            break;
        }
    }

    va_end(ap);

    // Always terminated when there is room for anything; size 0 writes nothing
    if (size > 0)
        str[out.length < size ? out.length : size - 1] = '\0';

    return (int)out.length;
}