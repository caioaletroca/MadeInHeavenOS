#include <stdio.h>
#include <stdtest.h>

#define SPECIFIER_NONE  0
#define SPECIFIER_FLAG  1
#define SPECIFIER_TYPE  4


#define CHAR_SCP 'c'

static size_t uimaxtoa(unsigned long integer, char *buffer, int base, int uppercase, size_t size);

static size_t imaxtoa(long integer, char *buffer, int base, int uppercase, size_t size);

int vsnprintf(char *str, size_t size, const char *format, va_list args) {
    // Setting initial values
    int f = 0;
    int s = 0;
    int n = 0;
    int remain = size - 1;
    int specifier = SPECIFIER_NONE;

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
                    specifier = SPECIFIER_TYPE;
                    f++;
                }
                continue;

            // Specifier found, check for type
            case SPECIFIER_TYPE:
                n = 0;
                switch (format[f])
                {
                    // Decimal integer numbers
                    case 'd':
                    case 'i':
                        value_i = va_arg(args, int);
                        n = imaxtoa(value_i, &str[s], 10, 0, remain);
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
                    default:
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
    if(integer > 0)
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