#include <stdio.h>

int vprintf(const char *format, va_list args) {
    return vfprintf(stdout, format, args);
}