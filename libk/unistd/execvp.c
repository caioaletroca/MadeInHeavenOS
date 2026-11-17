#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define PATH_CANDIDATE_MAX 256

int execvp(const char *name, char *const argv[])
{
    if (strchr(name, '/') != NULL)
        return execv(name, argv);

    const char *dir = getenv("PATH");
    if (dir == NULL)
    {
        errno = ENOENT;
        return -1;
    }

    char candidate[PATH_CANDIDATE_MAX];
    int denied = 0;
    while (1)
    {
        const char *end = strchr(dir, ':');
        size_t length = end != NULL ? (size_t)(end - dir) : strlen(dir);

        // An empty entry would mean the current directory (POSIX): none exist yet
        if (length > 0)
        {
            int n = snprintf(candidate, sizeof(candidate), "%.*s/%s", (int)length, dir, name);

            // Cut off: no such program can exist under this entry
            if (n >= 0 && (size_t)n < sizeof(candidate))
            {
                execv(candidate, argv);

                if (errno == EACCES)
                    denied = 1;
                else if (errno != ENOENT)
                    return -1;
            }
        }

        if (end == NULL)
            break;
        dir = end + 1;
    }

    errno = denied ? EACCES : ENOENT;
    return -1;
}