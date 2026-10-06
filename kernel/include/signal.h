#ifndef _SIGNAL_H
#define _SIGNAL_H

// Signal numbers (Linux values); no delivery yet, only used to encode exit statuses
#define SIGILL 4
#define SIGTRAP 5
#define SIGBUS 7
#define SIGFPE 8
#define SIGKILL 9
#define SIGSEGV 11

// Exit status of a process killed by a signal (shell convention: 128 + signal)
#define SIGNAL_EXIT_STATUS(signal) (128 + (signal))

#endif // _SIGNAL_H
