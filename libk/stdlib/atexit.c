#include <stdlib.h>
#include <stddef.h>
#include <libc.h>

// Maximum number of atexit handlers that can be registered.
#define ATEXIT_MAX 32

typedef void (*function_t)(void *);

/**
 * Structure representing an atexit handler entry.
 */
typedef struct entry
{
    function_t function;
    void *arg;
} entry_t;

// g++ passes &__dso_handle to __cxa_atexit to say which shared object a
// destructor belongs to. Static programs are one object: it is never compared.
void *__dso_handle = &__dso_handle;

static entry_t entries[ATEXIT_MAX];
static size_t entries_count = 0;

/**
 * Registers a function to be called at program exit.
 *
 * @param function The function to register.
 * @param arg The argument to pass to the function.
 * @param dso_handle The dynamic shared object handle (unused).
 * @return 0 on success, -1 if the maximum number of handlers is reached.
 */
int __cxa_atexit(function_t function, void *arg, void *dso_handle)
{
    (void)dso_handle; // Unused parameter

    if (entries_count >= ATEXIT_MAX)
        return -1;

    entries[entries_count].function = function;
    entries[entries_count].arg = arg;
    entries_count++;
    return 0;
}

int atexit(void (*function)(void))
{
    // Called later as function(NULL): the extra argument goes in a register the
    // function never reads. Not strictly portable C, fine on x86-64 (musl does the same)
    return __cxa_atexit((function_t)function, NULL, NULL);
}

void __run_exit_handlers(void)
{
    while (entries_count > 0)
    {
        entry_t entry = entries[--entries_count];
        entry.function(entry.arg);
    }
}