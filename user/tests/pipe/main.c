#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <spawn.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <test.h>

/*
 * Pipes: the two ends, data in order across the ring buffer's wrap, short
 * reads, EOF once every write end is closed, EPIPE once every read end is,
 * and the calls a pipe refuses. Blocking needs a second process: the test
 * spawns itself as a reader (stdin from the pipe) or a writer (stdout into
 * it). A child inherits every fd, so it closes the end it does not use (its
 * number in argv[2]), or the other side would never see EOF.
 */

#define PIPE_SIZE 4096 // the kernel's buffer
#define FILES_MAX 16   // PROCESS_MAX_FILES
#define BIG 20000      // several buffers' worth: the writer must wait for the reader
#define WRITER_TEXT "written by the child, read by the parent\n"

static char *self;

// The byte at position i of the big transfer
static unsigned char pattern(size_t i)
{
    return (unsigned char)(i * 7 % 251);
}

static int write_all(int fd, const void *data, size_t size)
{
    const char *bytes = data;
    for (size_t done = 0; done < size;)
    {
        ssize_t n = write(fd, bytes + done, size - done);
        if (n <= 0)
            return -1;
        done += (size_t)n;
    }
    return 0;
}

// Reads until size bytes or EOF; returns the count, or -1
static long read_full(int fd, void *data, size_t size)
{
    char *bytes = data;
    size_t done = 0;
    while (done < size)
    {
        ssize_t n = read(fd, bytes + done, size - done);
        if (n < 0)
            return -1;
        if (n == 0)
            break;
        done += (size_t)n;
    }
    return (long)done;
}

// Spawns this program as `mode`, told the fd it must close; returns the pid or -1
static pid_t child_spawn(const char *mode, int unused_fd)
{
    char number[12];
    snprintf(number, sizeof(number), "%d", unused_fd);
    char *argv[] = {self, (char *)mode, number, NULL};
    pid_t pid = -1;
    return posix_spawn(&pid, self, NULL, NULL, argv, environ) == 0 ? pid : -1;
}

static int child_exit_status(pid_t pid)
{
    int status;
    if (pid <= 0 || waitpid(pid, &status, 0) != pid || !WIFEXITED(status))
        return -1;
    return WEXITSTATUS(status);
}

static void test_ends(void)
{
    int fds[2] = {-1, -1};
    CHECK(pipe(fds) == 0);
    CHECK(fds[0] == 3 && fds[1] == 4); // the two lowest free fds, read end first

    // One pipe, two ends: a FIFO, 0600, the same inode on its own file system
    struct stat r, w, root;
    CHECK(fstat(fds[0], &r) == 0 && fstat(fds[1], &w) == 0 && stat("/", &root) == 0);
    CHECK(S_ISFIFO(r.st_mode) && !S_ISREG(r.st_mode) && (r.st_mode & 0777) == 0600);
    CHECK(r.st_ino == w.st_ino && r.st_dev == w.st_dev && r.st_mode == w.st_mode);
    CHECK(r.st_dev != root.st_dev && r.st_size == 0);

    // A second pipe is another inode
    int other[2];
    CHECK(pipe(other) == 0);
    struct stat o;
    CHECK(fstat(other[0], &o) == 0 && o.st_ino != r.st_ino && o.st_dev == r.st_dev);
    close(other[0]);
    close(other[1]);

    // Each end works one way only
    char byte = 'x';
    errno = 0;
    CHECK(read(fds[1], &byte, 1) == -1 && errno == EBADF);
    errno = 0;
    CHECK(write(fds[0], &byte, 1) == -1 && errno == EBADF);

    close(fds[0]);
    close(fds[1]);
}

static void test_data(void)
{
    int fds[2];
    CHECK(pipe(fds) == 0);

    // A read takes what is there, without waiting for the rest of its count
    char buffer[100];
    CHECK(write(fds[1], "hello", 5) == 5);
    CHECK(read(fds[0], buffer, sizeof(buffer)) == 5 && memcmp(buffer, "hello", 5) == 0);

    // Two writes come out in order, and a small read leaves the rest
    CHECK(write(fds[1], "ab", 2) == 2 && write(fds[1], "cd", 2) == 2);
    CHECK(read(fds[0], buffer, 3) == 3 && memcmp(buffer, "abc", 3) == 0);
    CHECK(read(fds[0], buffer, 3) == 1 && buffer[0] == 'd');

    // Three rounds of 3000 bytes cross the buffer's end: the ring wraps, the bytes stay in order
    static unsigned char out[3000], in[3000];
    int same = 1;
    for (int round = 0; round < 3; round++)
    {
        for (size_t i = 0; i < sizeof(out); i++)
            out[i] = pattern(round * sizeof(out) + i);
        CHECK(write_all(fds[1], out, sizeof(out)) == 0);
        CHECK(read_full(fds[0], in, sizeof(in)) == (long)sizeof(in));
        same = same && memcmp(in, out, sizeof(in)) == 0;
    }
    CHECK(same);

    // Exactly a full buffer fits without a reader
    static unsigned char full[PIPE_SIZE];
    memset(full, 'f', sizeof(full));
    CHECK(write_all(fds[1], full, sizeof(full)) == 0);
    CHECK(read_full(fds[0], full, sizeof(full)) == PIPE_SIZE);

    close(fds[0]);
    close(fds[1]);
}

static void test_eof_and_epipe(void)
{
    // The write end closed: what is buffered is still read, then 0, again and again
    int fds[2];
    CHECK(pipe(fds) == 0);
    CHECK(write(fds[1], "last", 4) == 4);
    CHECK(close(fds[1]) == 0);
    char buffer[16];
    CHECK(read(fds[0], buffer, sizeof(buffer)) == 4 && memcmp(buffer, "last", 4) == 0);
    CHECK(read(fds[0], buffer, sizeof(buffer)) == 0);
    CHECK(read(fds[0], buffer, sizeof(buffer)) == 0);
    close(fds[0]);

    // The read end closed: writing is EPIPE (main ignores SIGPIPE; the signal test has it)
    CHECK(pipe(fds) == 0);
    CHECK(close(fds[0]) == 0);
    errno = 0;
    CHECK(write(fds[1], "x", 1) == -1 && errno == EPIPE);
    close(fds[1]);

    // A dup'd read end keeps the pipe readable: only the last close counts
    CHECK(pipe(fds) == 0);
    int copy = dup(fds[0]);
    CHECK(close(fds[0]) == 0);
    CHECK(write(fds[1], "y", 1) == 1);
    CHECK(read(copy, buffer, 1) == 1 && buffer[0] == 'y');
    close(copy);
    close(fds[1]);
}

/*
 * The parent writes BIG bytes into a child reading its stdin: the parent waits
 * whenever the buffer is full, the child whenever it is empty, and the child sees
 * EOF once the parent closes the write end. The child checks every byte.
 */
static void test_reader_child(void)
{
    int fds[2];
    CHECK(pipe(fds) == 0);

    // The child's stdin is the read end; the parent's own stdin comes back after the spawn
    int saved = dup(0);
    CHECK(dup2(fds[0], 0) == 0);
    pid_t pid = child_spawn("reader", fds[1]);
    CHECK(dup2(saved, 0) == 0);
    close(saved);
    close(fds[0]);
    CHECK(pid > 0);

    static unsigned char data[BIG];
    for (size_t i = 0; i < sizeof(data); i++)
        data[i] = pattern(i);
    CHECK(write_all(fds[1], data, sizeof(data)) == 0);
    CHECK(close(fds[1]) == 0); // the last write end: the child's next read is 0

    CHECK(child_exit_status(pid) == 0);
    CHECK(isatty(0)); // stdin is the console again
}

static int reader_main(void)
{
    static unsigned char data[BIG + 1];
    long n = read_full(0, data, sizeof(data)); // one byte more than sent: stops at EOF
    if (n != BIG)
        return 1;
    for (size_t i = 0; i < BIG; i++)
        if (data[i] != pattern(i))
            return 2;
    return 0;
}

// The child writes into its stdout, the pipe's write end; the parent reads it all, then EOF
static void test_writer_child(void)
{
    int fds[2];
    CHECK(pipe(fds) == 0);

    int saved = dup(1);
    CHECK(dup2(fds[1], 1) == 1);
    pid_t pid = child_spawn("writer", fds[0]);
    int restored = dup2(saved, 1) == 1; // CHECK prints to stdout: only after it is back
    close(saved);
    close(fds[1]);
    CHECK(restored && pid > 0);

    char buffer[128];
    long n = read_full(fds[0], buffer, sizeof(buffer));
    CHECK(n == (long)strlen(WRITER_TEXT) && memcmp(buffer, WRITER_TEXT, (size_t)n) == 0);
    CHECK(child_exit_status(pid) == 0);
    close(fds[0]);
}

static int writer_main(void)
{
    // In two writes, so the parent may see them apart
    size_t half = strlen(WRITER_TEXT) / 2;
    if (write_all(1, WRITER_TEXT, half) != 0 || write_all(1, WRITER_TEXT + half, strlen(WRITER_TEXT) - half) != 0)
        return 1;
    return 0;
}

/*
 * A full pipe whose last reader is a child: the parent's next write waits for
 * room, and the child's exit (closing that reader) ends the wait with EPIPE.
 * If the child exits first, the write fails at once: EPIPE either way.
 */
static void test_writer_woken_by_last_reader(void)
{
    int fds[2];
    CHECK(pipe(fds) == 0);

    static unsigned char full[PIPE_SIZE];
    CHECK(write_all(fds[1], full, sizeof(full)) == 0);

    pid_t pid = child_spawn("drop", fds[1]); // holds the read end until it exits
    close(fds[0]);
    CHECK(pid > 0);

    errno = 0;
    CHECK(write(fds[1], "z", 1) == -1 && errno == EPIPE);
    CHECK(child_exit_status(pid) == 0);
    close(fds[1]);
}

static void test_refused(void)
{
    int fds[2];
    CHECK(pipe(fds) == 0);

    errno = 0;
    CHECK(lseek(fds[0], 0, SEEK_SET) == -1 && errno == ESPIPE);
    errno = 0;
    CHECK(lseek(fds[1], 0, SEEK_CUR) == -1 && errno == ESPIPE);
    errno = 0;
    CHECK(ftruncate(fds[1], 0) == -1 && errno == EINVAL);
    char records[64];
    errno = 0;
    CHECK(getdents(fds[0], records, sizeof(records)) == -1 && errno == ENOTDIR);
    struct winsize size;
    errno = 0;
    CHECK(ioctl(fds[0], TIOCGWINSZ, &size) == -1 && errno == ENOTTY);
    errno = 0;
    CHECK(isatty(fds[0]) == 0 && errno == ENOTTY);

    close(fds[0]);
    close(fds[1]);

    // A bad pointer: EFAULT, and both fds are given back
    errno = 0;
    CHECK(pipe((int *)1) == -1 && errno == EFAULT);
    int fd = open("/", O_RDONLY);
    CHECK(fd == 3);
    close(fd);
}

// One free fd is not enough: EMFILE, and the free one is still free afterwards
static void test_table_full(void)
{
    int fds[FILES_MAX];
    int copies = 0;
    while (copies < FILES_MAX - 4) // 3 + 12 taken: one left
        fds[copies++] = dup(0);
    CHECK(fds[copies - 1] == FILES_MAX - 2);

    int pair[2];
    errno = 0;
    CHECK(pipe(pair) == -1 && errno == EMFILE);
    int last = dup(0);
    CHECK(last == FILES_MAX - 1);
    close(last);

    for (int i = 0; i < copies; i++)
        close(fds[i]);
}

int main(int argc, char **argv)
{
    self = argv[0];

    // Before the child modes too: this test is about the error, and exec keeps an ignored signal
    signal(SIGPIPE, SIG_IGN);

    if (argc > 2)
    {
        close(atoi(argv[2])); // the end this child does not use
        if (strcmp(argv[1], "reader") == 0)
            return reader_main();
        if (strcmp(argv[1], "writer") == 0)
            return writer_main();
        if (strcmp(argv[1], "drop") == 0)
            return 0;
        return 100;
    }

    test_ends();
    test_data();
    test_eof_and_epipe();
    test_reader_child();
    test_writer_child();
    test_writer_woken_by_last_reader();
    test_refused();
    test_table_full();

    // Every fd was closed: the next open gets fd 3 again
    int fd = open("/", O_RDONLY);
    CHECK(fd == 3);
    close(fd);

    return test_status();
}
