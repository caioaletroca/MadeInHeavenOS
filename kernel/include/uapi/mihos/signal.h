#ifndef _UAPI_MIHOS_SIGNAL_H_
#define _UAPI_MIHOS_SIGNAL_H_

#include <stdint.h>

#define NSIG 32 // valid signals are 1..31

#define SIGHUP 1
#define SIGINT 2
#define SIGQUIT 3
#define SIGILL 4
#define SIGTRAP 5
#define SIGABRT 6
#define SIGBUS 7
#define SIGFPE 8
#define SIGKILL 9
#define SIGUSR1 10
#define SIGSEGV 11
#define SIGUSR2 12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20
#define SIGTTIN 21
#define SIGTTOU 22
#define SIGURG 23
#define SIGWINCH 28

#define SIG_DFL ((void (*)(int))0)    // the signal's default action
#define SIG_IGN ((void (*)(int))1)    // do nothing
#define SIG_ERR ((void (*)(int)) - 1) // signal() failed

// sa_flags
#define SA_RESTART 0x1   // system calls continue after the handler, no EINTR
#define SA_NODEFER 0x2   // the handler's own signal is not blocked while it runs
#define SA_RESETHAND 0x4 // back to SIG_DFL once the handler is entered

// sigprocmask's how
#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

// sa_flags (Linux's values)
#define SA_RESTART 0x10000000   // system calls continue after the handler, no EINTR
#define SA_NODEFER 0x40000000   // the handler's own signal is not blocked while it runs
#define SA_RESETHAND 0x80000000 // back to SIG_DFL once the handler is entered
#define SA_RESTORER 0x04000000  // sa_restorer is set (libc always sets it; the kernel requires it)

typedef uint64_t sigset_t; // bit n-1 = signal n (Linux's layout)

#define MIHOS_SIGNAL_BIT(signal) (1ull << ((signal) - 1))

struct sigaction
{
    void (*sa_handler)(int);
    unsigned long sa_flags;    // SA_*
    void (*sa_restorer)(void); // set by libc: where the handler returns to
    sigset_t sa_mask;          // also blocked while the handler runs
};

#define SIGNAL_EXIT_STATUS(signal) (128 + (signal))

#endif /* _UAPI_MIHOS_SIGNAL_H_ */