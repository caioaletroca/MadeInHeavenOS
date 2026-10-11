#include <stdlib.h>
#include <unistd.h>
#include <FILE.h>
#include <libc.h>

__attribute__((__noreturn__)) void exit(int status)
{
    // atexit handlers and C++ global destructors (both through __cxa_atexit), newest first
    __run_exit_handlers();

    // __attribute__((destructor)) functions and .fini_array, also newest first
    for (size_t i = (size_t)(__fini_array_end - __fini_array_start); i > 0; i--)
        __fini_array_start[i - 1]();

    // Last: anything the handlers and destructors printed still gets flushed
    __stdio_exit();
    _exit(status);
}