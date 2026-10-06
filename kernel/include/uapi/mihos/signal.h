#ifndef _UAPI_MIHOS_SIGNAL_H_
#define _UAPI_MIHOS_SIGNAL_H_

#define SIGILL 4
#define SIGTRAP 5
#define SIGBUS 7
#define SIGFPE 8
#define SIGKILL 9
#define SIGSEGV 11

#define SIGNAL_EXIT_STATUS(signal) (128 + (signal))

#endif /* _UAPI_MIHOS_SIGNAL_H_ */