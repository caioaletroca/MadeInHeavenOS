#include <FILE.h>
#include <fcntl.h>

unsigned int __mode_flags(const char *mode, int *open_flags)
{
    unsigned int flags;

    switch (mode[0])
    {
    case 'r':
        flags = F_READ;
        *open_flags = O_RDONLY;
        break;
    case 'w':
        flags = F_WRITE;
        *open_flags = O_WRONLY | O_CREAT | O_TRUNC;
        break;
    case 'a':
        flags = F_WRITE;
        *open_flags = O_WRONLY | O_CREAT | O_APPEND;
        break;
    default:
        return 0;
    }

    for (const char *m = mode + 1; *m != '\0'; m++)
    {
        // both ways; r+ still neither creates nor empties
        if (*m == '+')
        {
            flags |= F_READ | F_WRITE;
            *open_flags = (*open_flags & ~O_ACCMODE) | O_RDWR;
        }
        // C11 "wx": fail with EEXIST if it exists
        else if (*m == 'x' && (*open_flags & O_CREAT))
        {
            *open_flags |= O_EXCL;
        }
        // 'b' (binary) means nothing on POSIX; anything else is ignored, as glibc
    }

    return flags;
}