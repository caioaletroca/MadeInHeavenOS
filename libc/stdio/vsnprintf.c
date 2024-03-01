#include <stdio.h>
#include <stdtest.h>

#define SPECIFIER_NONE      0
#define SPECIFIER_FLAGS     1
#define SPECIFIER_WITDH     2
#define SPECIFIER_PRECISION 3
#define SPECIFIER_TYPE      4

#define FLAG_ZERO       (1 << 4)

#define CHAR_SCP 'c'

static size_t uimaxtoa(unsigned long integer, char *buffer, int base, int uppercase, size_t size);

static size_t imaxtoa(long integer, char *buffer, int base, int uppercase, size_t size);

static size_t uimaxtoa_fill(long integer, char *buffer, int base, int uppercase, int width, int filler, int max);

int vsnprintf(char *str, size_t size, const char *format, va_list args) {
    // Setting initial values
    int f = 0;
    int s = 0;
    int n = 0;
    int remain = size - 1;
    int specifier = SPECIFIER_NONE;
    int flags = 0;
    int alignment;
    int filler;
    int w = 0;
    // TODO: Use proper macros
    size_t width;
    size_t str_size = (size > (512 / 2) ? (512 / 2) : size);
    char width_str[str_size];

    signed long value_i;
    unsigned long value_ui;
    char *value_cp;

    char *test;
    
    // Travel through the format
    while (format[f] != 0 && s < (size - 1)) {
        switch (specifier) {
            // In case any no specifiers was found
            case SPECIFIER_NONE:
                // Check for specifiers start
                if(format[f] != '%') {
                    // No specifier start found
                    str[s++] = format[f++];
                    remain--;
                }
                // Check for % scape character
                else if(format[f + 1] == '%') {
                    str[s++] = '%';
                    f += 2;
                    remain--;
                }
                // Specifier start found
                else {
                    specifier = SPECIFIER_FLAGS;
                    f++;
                }
                continue;

            case SPECIFIER_FLAGS:
                switch (format[f++])
                {
                    case '0':
                        flags |= FLAG_ZERO;
                        break;
                    
                    default:
                        f--;
                        specifier = SPECIFIER_WITDH;
                        break;
                }
                break;

            case SPECIFIER_WITDH:
                w = 0;
                while('0' <= format[f] &&format[f] <= '9' && w < str_size) {
                    width_str[w++] = format[f++];
                }
                width_str[w] = '\0';

                if(format[f] == '.') {
                    specifier = SPECIFIER_PRECISION;
                    f++;
                    panic("vsnprintf.c: Specifier Precision not implemented");
                } else {
                    specifier = SPECIFIER_TYPE;
                }
                break;

            // TODO: Implement Specifier Precision

            // Specifier found, check for type
            case SPECIFIER_TYPE:
                n = 0;
                width = atoi(width_str);
                filler = (flags & FLAG_ZERO) ? '0' : ' ';
                alignment = width;
                switch (format[f])
                {
                    // Decimal integer numbers
                    case 'd':
                    case 'i':
                        value_i = va_arg(args, int);
                        n = imaxtoa(value_i, &str[s], 10, 0, remain);
                        f++;
                        break;
                    // Unsigned decimal integer numbers
                    case 'u':
                        value_i = va_arg(args, unsigned int);
                        n = uimaxtoa(value_i, &str[s], 10, 0, remain);
                        f++;
                        break;
                    // Pointer address
                    case 'p':
                        filler = '0';
                        alignment = 2 * sizeof(void *);
                    // Hexadecimal
                    case 'x':
                        value_i = va_arg(args, unsigned int);
                        n = uimaxtoa_fill(value_i, &str[s], 16, 0, alignment, filler, remain);
                        f++;
                        break;
                    // Uppercase Hexadecimal
                    case 'X':
                        value_i = va_arg(args, unsigned int);
                        n = uimaxtoa_fill(value_i, &str[s], 16, 1, alignment, filler, remain);
                        f++;
                        break;
                    // Strings
                    case 's':
                        value_cp = va_arg(args, char *);
                        n = strlen(value_cp);
                        memcpy(str + s, value_cp, n);
                        f++;
                        break;
                    // Chars
                    case 'c':
                        value_ui = va_arg(args, unsigned int);
                        str[s++] = (char)value_ui;
                        f++;
                        break;
                    // Panic
                    default:
                        panic("vsnprintf.c: Unknow specifier %%%c at string '%s'.", format[f], str);
                        break;
                }
                specifier = SPECIFIER_NONE;
                s += n;
                remain -= n;
            default:
                break;
        }
    }
    str[s] = '\0';
    return s;
}

/**
 * @brief Convert a maximum rank unsigned integer into a string
 * 
 * @param integer The integer
 * @param buffer The output string
 * @param base Numeric base
 * @param uppercase If hexadecimal, if the number should be uppercase
 * @param size The maximum size for the convertion
 * @return size_t The number of digits from the output string
 */
static size_t uimaxtoa(unsigned long integer, char *buffer, int base, int uppercase, size_t size) {
    unsigned long integer_copy = integer;
    size_t digits;
    int i;
    unsigned char remainder;

    // Count the number of digits needed
    for(digits = 0; integer_copy > 0; digits++)
        integer_copy = integer_copy / base;

    // Handler null buffer cases
    if(buffer == NULL && integer == 0)
        return 1;
    if(buffer == NULL && integer > 0)
        return digits;

    // Handler zero case
    if(integer == 0) {
        buffer[0] = '0';
        return 1;
    }

    // Adjust the maximum number of digits
    if(size > 0 && digits > size)
        digits = size;

    // Iterate over the digits in backyard direction in the buffer
    for(i = digits - 1; integer != 0 && i >= 0; i--) {
        remainder = integer % base;
        integer = integer / base;

        // For decimal numbers
        if(remainder <= 9) {
            *(buffer + i) = remainder + '0';
        }
        // For hexadecimal numbers
        else {
            if(uppercase)
                *(buffer + i) = remainder - 10 + 'A';
            else
                *(buffer + i) = remainder - 10 + 'a';
        }
    }

    return digits;
}

/**
 * @brief Convert a maximum rank number with sign into a string
 * 
 * @param integer The integer
 * @param buffer The output string
 * @param base Numeric base
 * @param uppercase If hexadecimal, if the number should be uppercase
 * @param size The maximum size for the convertion
 * @return size_t The number of digits from the output string
 */
static size_t imaxtoa(long integer, char *buffer, int base, int uppercase, size_t size) {
    // If the number is positive, just make the simple convertion
    if(integer >= 0)
        return uimaxtoa(integer, buffer, base, uppercase, size);

    // Handle the null buffer case
    if(buffer == NULL)
        return uimaxtoa(-integer, NULL, base, uppercase, size) + 1;

    // Insert the minus sign
    *buffer = '-';

    // Handle the not enough space case
    if(size == 1) {
        return 1;
    }
    else {
        // Invert the integer to handle as a positive number
        // Pass to the function the second buffer position
        return uimaxtoa(-integer, buffer + 1, base, uppercase, size - 1) + 1;
    }
}

/*
 * Convert a maximum rank integer without sign into a string, taking care of
 * the alignment.
 */
static size_t uimaxtoa_fill(long integer, char *buffer, int base, int uppercase, int width, int filler, int size) {
    size_t size_int;
    size_t size_filler;

    // Handle the not enough space case
    if(size < 0) {
        return 0;
    }
    
    // Calculate the number of characters needed for this number
    size_int = uimaxtoa(integer, NULL, base, uppercase, 0);
    
    // Cap the max width according to the remaining size available
    if(width > 0 && size > 0 && width > size)
        width = size;
    if(width < 0 && -size < 0 && width < -size)
        width = -size;

    // If the integer is bigger than the padding, cap the value
    if(size_int > abs(width))
        return uimaxtoa(integer, buffer, base, uppercase, abs(width));

    // If the width is zero for any reason and we have space, do nothing
    if(width == 0 && size > 0)
        return uimaxtoa(integer, buffer, base, uppercase, size);

    // If the width is zero for any reason and we don't have space, cap the value
    if(width == 0)
        return uimaxtoa(integer, buffer, base, uppercase, abs(width));

    // Calculate the number of fillers needed
    size_filler = abs(width) - size_int;
    
    if (width < 0) {
        // Left alignment
        uimaxtoa(integer, buffer, base, uppercase, 0);
        memset(buffer + size_int, filler, size_filler);
    } else {
        // Right alignment
        memset(buffer, filler, size_filler);
        uimaxtoa(integer, buffer + size_filler, base, uppercase, 0);
    }

    return abs(width);
}