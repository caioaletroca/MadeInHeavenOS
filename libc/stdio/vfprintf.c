#include <stdio.h>

int vfprintf(FILE *stream, const char *format, va_list args) {
    int n, w, left;
    char buf[BUFFER_SIZE];
    char *pointer = buf;

    // Try to parse the format, if return is lower than 0;
    // returns error
    if((n = vsnprintf(buf, BUFFER_SIZE, format, args)) < 0) {
        return n;
    }

    left = n;

    // While there's characters left, write into stream
    while(left > 0) {
        w = fwrite(pointer, 1, n, stream);

        // If error happens, break
        if(w < 0)
            break;

        left -= w;
        pointer += w;
    }

    return n - left;
}