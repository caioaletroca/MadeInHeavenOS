#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define CHUNK 4096

/**
 * @brief Copy the contents of the given file descriptor to standard output.
 *
 * @param fd The file descriptor to copy from.
 * @return 0 on success, -1 on error.
 */
static int copy(int fd)
{
    static char buffer[CHUNK];
    while (1)
    {
        ssize_t n = read(fd, buffer, sizeof(buffer));

        if (n == 0)
            return 0;
        if (n < 0)
            return -1;

        for (ssize_t done = 0; done < n;)
        {
            ssize_t written = write(1, buffer + done, (size_t)(n - done));
            if (written < 0)
                return -1;
            done += written;
        }
    }
}

int main(int argc, char *argv[])
{
    char *stdin_only[] = {argv[0], "-", NULL};
    if (argc == 1)
    {
        argc = 2;
        argv = stdin_only;
    }

    int status = EXIT_SUCCESS;
    for (int i = 1; i < argc; i++)
    {
        const char *name = argv[i];
        int fd = strcmp(name, "-") == 0 ? 0 : open(name, O_RDONLY);
        if (fd < 0 || copy(fd) < 0)
        {
            fprintf(stderr, "cat: %s: %s\n", name, strerror(errno));
            status = EXIT_FAILURE;
        }

        if (fd > 0)
            close(fd);
    }

    return status;
}