#include <FILE.h>
#include <unistd.h>

// TODO: retry on EINTR once signals exist
size_t __write_all(FILE *f, const unsigned char *data, size_t n)
{
    size_t done = 0;

    while (done < n)
    {
        ssize_t written = write(f->fd, data + done, n - done);
        if (written <= 0)
        {
            f->flags |= F_ERR;
            break;
        }
        done += (size_t)written;
    }

    return done;
}