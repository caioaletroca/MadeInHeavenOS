#include <fs/pipe.h>
#include <fs/file.h>
#include <fs/vfs.h>
#include <sched/wait.h>
#include <mm/kmalloc.h>
#include <sys/list.h>
#include <mihos/errno.h>

#define PIPE_SIZE 4096   // the buffer: one page (Linux: 16)
#define PIPE_ATOMIC 4096 // POSIX's PIPE_BUF: a write this small never interleaves with another

typedef struct pipe
{
    vfs_node_t base;       // what fstat sees: VFS_FIFO, 0600, the pipes' st_dev, its own ino
    spinlock_t lock;       // everything below
    char *buffer;          // PIPE_SIZE bytes, a ring
    uint64_t read_count;   // bytes ever read: the next one is buffer[read_count % PIPE_SIZE]
    uint64_t write_count;  // bytes ever written: write_count - read_count are waiting
    unsigned int readers;  // open read ends (file_ts, not fds: dup and spawn share one)
    unsigned int writers;  // open write ends
    wait_queue_t readable; // readers waiting for data, or for the last writer to go
    wait_queue_t writable; // writers waiting for room, or for the last reader to go
} pipe_t;

#define PIPE(n) list_container(n, pipe_t, base)

static uint64_t pipe_dev;
static uint64_t pipe_next_ino = 1;
static spinlock_t pipe_ino_lock;

/**
 * @brief Reads data from a pipe into the provided buffer.
 *
 * @param file The file representing the pipe to read from.
 * @param buf The buffer to store the read data.
 * @param count The maximum number of bytes to read.
 * @return The number of bytes read, or 0 if no data is available and no writers remain.
 */
static long pipe_read(file_t *file, char *buf, size_t count)
{
    pipe_t *pipe = file->private;
    if (count == 0)
        return 0;

    guard(spinlock, &pipe->lock);
    while (pipe->read_count == pipe->write_count && pipe->writers > 0)
    {
        int interrupted = wait_queue_sleep_interruptible(&pipe->readable, &pipe->lock);
        if (interrupted < 0)
            return interrupted;
    }

    size_t done = 0;
    while (done < count && pipe->read_count != pipe->write_count)
        buf[done++] = pipe->buffer[pipe->read_count++ % PIPE_SIZE];

    if (done > 0)
        wait_queue_wake_all(&pipe->writable);

    return (long)done;
}

/**
 * @brief Writes data to a pipe from the provided buffer.
 *
 * @param file The file representing the pipe to write to.
 * @param buf The buffer containing the data to write.
 * @param count The number of bytes to write.
 * @return The number of bytes written, or -EPIPE if no readers remain.
 */
static long pipe_write(file_t *file, const char *buf, size_t count)
{
    pipe_t *pipe = file->private;
    guard(spinlock, &pipe->lock);

    size_t done = 0;
    while (done < count)
    {
        size_t need = count <= PIPE_ATOMIC ? count : 1;
        while (pipe->readers > 0 && PIPE_SIZE - (pipe->write_count - pipe->read_count) < need)
        {
            int interrupted = wait_queue_sleep_interruptible(&pipe->writable, &pipe->lock);
            if (interrupted < 0)
                return done > 0 ? (long)done : interrupted;
        }

        if (pipe->readers == 0)
            return done > 0 ? (long)done : -EPIPE;

        while (done < count && pipe->write_count - pipe->read_count < PIPE_SIZE)
            pipe->buffer[pipe->write_count++ % PIPE_SIZE] = buf[done++];

        wait_queue_wake_all(&pipe->readable);
    }

    return (long)done;
}

/**
 * @brief Releases a pipe file, updating reader and writer counts.
 *
 * @param file The file representing the pipe to release.
 */
static void pipe_file_release(file_t *file)
{
    pipe_t *pipe = file->private;
    guard(spinlock, &pipe->lock);

    if (file->flags & FILE_READ)
    {
        pipe->readers--;
        wait_queue_wake_all(&pipe->writable);
    }
    if (file->flags & FILE_WRITE)
    {
        pipe->writers--;
        wait_queue_wake_all(&pipe->readable);
    }
}

/**
 * @brief Releases a pipe node, freeing its resources.
 *
 * @param node The VFS node representing the pipe to release.
 */
static void pipe_node_release(vfs_node_t *node)
{
    pipe_t *pipe = PIPE(node);
    kfree(pipe->buffer);
    kfree(pipe);
}

static const file_ops_t pipe_file_ops = {
    .read = pipe_read,
    .write = pipe_write,
    .release = pipe_file_release,
};

static const vfs_node_ops_t pipe_node_ops = {
    .release = pipe_node_release,
};

int pipe_create(file_t **read_end, file_t **write_end)
{
    pipe_t *pipe = kzalloc(sizeof(*pipe));
    char *buffer = kmalloc(PIPE_SIZE);
    file_t *reader = file_create(&pipe_file_ops, FILE_READ, pipe);
    file_t *writer = file_create(&pipe_file_ops, FILE_WRITE, pipe);

    if (pipe == NULL || buffer == NULL || reader == NULL || writer == NULL)
    {
        kfree(buffer);
        kfree(pipe);
        kfree(reader);
        kfree(writer);
        return -ENOMEM;
    }

    vfs_node_init(&pipe->base, VFS_FIFO, &pipe_node_ops);
    pipe->base.mode = 0600;
    pipe->base.dev = pipe_dev;
    scoped_guard(spinlock, &pipe_ino_lock)
        pipe->base.ino = pipe_next_ino++;

    spinlock_init(&pipe->lock);
    pipe->buffer = buffer;
    pipe->readers = 1;
    pipe->writers = 1;
    wait_queue_init(&pipe->readable);
    wait_queue_init(&pipe->writable);

    reader->node = &pipe->base;
    vfs_node_get(&pipe->base);
    writer->node = &pipe->base;

    *read_end = reader;
    *write_end = writer;

    return 0;
}

void pipe_init(void)
{
    pipe_dev = vfs_dev_alloc();
    spinlock_init(&pipe_ino_lock);
}