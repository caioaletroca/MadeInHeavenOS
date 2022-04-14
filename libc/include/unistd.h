#ifndef _UNISTD_H_
#define _UNISTD_H_

#include <sys/cdefs.h>

__BEGIN_DECLS

int execv(const char *, char* const[]);
int execve(const char *, char* const[], char* const[]);
int execvp(const char *, char* const[]);
pid_t fork(void);

__END_DECLS

#endif