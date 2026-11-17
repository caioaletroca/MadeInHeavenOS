#include <unistd.h>
#include <syscall.h>
#include <errno.h>

char *getcwd(char *buffer, size_t size)
{
    if (size == 0)
    {
        errno = EINVAL;
        return NULL;
    }

    if (syscall(SYS_GETCWD, buffer, size) < 0)
        return NULL;

    return buffer;
}