#ifndef _INTERNAL_LIBC_H_
#define _INTERNAL_LIBC_H_

/*
 * libc's own lifecycle, shared between files and never installed: process
 * start (crt0 → __libc_start_main) and the exit path.
 */

// Called by crt0's _start: environ, global constructors, main, exit
__attribute__((noreturn)) void __libc_start_main(int argc, char **argv, char **envp);

// Runs the atexit/__cxa_atexit handlers in reverse order of registration (exit)
void __run_exit_handlers(void);

// Set by the linker script (user.ld): global constructors and destructors
extern void (*__init_array_start[])(void) __attribute__((visibility("hidden")));
extern void (*__init_array_end[])(void) __attribute__((visibility("hidden")));
extern void (*__fini_array_start[])(void) __attribute__((visibility("hidden")));
extern void (*__fini_array_end[])(void) __attribute__((visibility("hidden")));

#endif /* _INTERNAL_LIBC_H_ */