#include <stdlib.h>

__attribute__((__noreturn__))
void abort(void) {
    while(1) { }
    __builtin_unreachable();
}