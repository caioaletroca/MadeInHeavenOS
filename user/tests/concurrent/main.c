#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <test.h>

/*
 * Several processes in the same kernel code at once. System calls run with
 * IRQs on, so the timer can switch processes in the middle of one: in ext2,
 * the block cache, or an append. Each child works in /mnt/concurrent: it
 * writes its own files across block boundaries, reads them back and deletes
 * them, and appends tagged records to one shared O_APPEND file. Then every
 * record must be there once and whole. Not a proof that the locking is right,
 * but a missing lock shows here as a broken record or file, or in run-tests'
 * e2fsck after it.
 */

#define DIRECTORY "/mnt/concurrent"
#define SHARED DIRECTORY "/shared"
#define CHILDREN 4
#define ROUNDS 100
#define FILE_SIZE 3000 // three 1 KiB blocks, and a part of a fourth
#define RECORD_SIZE 64

static char pattern_byte(int child, int round, int i)
{
    return (char)((child * 31 + round * 7 + i) & 0xFF);
}

static void file_name(char *name, size_t size, int child, int round)
{
    snprintf(name, size, DIRECTORY "/c%d-%d", child, round);
}

// "c<child> r<round> " then the child's letter up to the newline
static void record_make(char *record, int child, int round)
{
    int length = snprintf(record, RECORD_SIZE, "c%d r%03d ", child, round);
    memset(record + length, 'a' + child, RECORD_SIZE - 1 - (size_t)length);
    record[RECORD_SIZE - 1] = '\n';
}

static long read_all(int fd, char *data, size_t size)
{
    size_t done = 0;
    while (done < size)
    {
        ssize_t n = read(fd, data + done, size - done);
        if (n <= 0)
            return n < 0 ? -1 : (long)done;
        done += (size_t)n;
    }
    return (long)done;
}

static void child_round(int child, int round, int shared)
{
    static char data[FILE_SIZE], back[FILE_SIZE];
    char name[64], record[RECORD_SIZE];

    for (int i = 0; i < FILE_SIZE; i++)
        data[i] = pattern_byte(child, round, i);

    file_name(name, sizeof(name), child, round);
    int fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    CHECK(fd >= 0);
    if (fd >= 0)
    {
        CHECK(write(fd, data, FILE_SIZE) == FILE_SIZE);
        close(fd);
    }

    fd = open(name, O_RDONLY);
    CHECK(fd >= 0);
    if (fd >= 0)
    {
        CHECK(read_all(fd, back, FILE_SIZE) == FILE_SIZE);
        CHECK(memcmp(back, data, FILE_SIZE) == 0);
        close(fd);
    }
    CHECK(unlink(name) == 0);

    record_make(record, child, round);
    CHECK(write(shared, record, RECORD_SIZE) == RECORD_SIZE);
}

static void child_run(int child)
{
    test_failures = 0; // a child reports only its own

    // Its own open file: its own position, so only O_APPEND keeps the records apart
    int shared = open(SHARED, O_WRONLY | O_APPEND);
    CHECK(shared >= 0);

    for (int round = 0; round < ROUNDS && shared >= 0; round++)
        child_round(child, round, shared);

    close(shared);
    fflush(stdout);
    _exit(test_status());
}

// Every record once, whole, in any order
static void check_records(void)
{
    static char data[CHILDREN * ROUNDS * RECORD_SIZE + 1];
    static unsigned char seen[CHILDREN][ROUNDS];
    char expected[RECORD_SIZE];

    int fd = open(SHARED, O_RDONLY);
    CHECK(fd >= 0);
    if (fd < 0)
        return;
    long size = read_all(fd, data, sizeof(data));
    close(fd);
    CHECK(size == CHILDREN * ROUNDS * RECORD_SIZE);
    if (size != CHILDREN * ROUNDS * RECORD_SIZE)
    {
        printf("  shared file: %ld bytes, expected %d\n", size, CHILDREN * ROUNDS * RECORD_SIZE);
        return;
    }

    int broken = 0;
    for (long offset = 0; offset < size; offset += RECORD_SIZE)
    {
        const char *record = data + offset;
        int child = record[1] - '0';
        int round = (record[4] - '0') * 100 + (record[5] - '0') * 10 + (record[6] - '0');

        if (record[0] != 'c' || child < 0 || child >= CHILDREN || round < 0 || round >= ROUNDS)
        {
            broken++;
            continue;
        }

        record_make(expected, child, round);
        if (memcmp(record, expected, RECORD_SIZE) != 0)
            broken++;
        else
            seen[child][round]++;
    }

    int wrong = 0;
    for (int child = 0; child < CHILDREN; child++)
        for (int round = 0; round < ROUNDS; round++)
            wrong += seen[child][round] != 1;

    if (broken != 0 || wrong != 0)
        printf("  shared file: %d broken records, %d records not there exactly once\n", broken, wrong);
    CHECK(broken == 0);
    CHECK(wrong == 0);
}

int main(void)
{
    CHECK(mkdir(DIRECTORY, 0755) == 0 || errno == EEXIST);
    int shared = open(SHARED, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    CHECK(shared >= 0);
    close(shared);

    pid_t children[CHILDREN];
    fflush(stdout);
    for (int child = 0; child < CHILDREN; child++)
    {
        children[child] = fork();
        if (children[child] == 0)
            child_run(child);
        CHECK(children[child] > 0);
    }

    for (int child = 0; child < CHILDREN; child++)
    {
        int status = -1;
        CHECK(children[child] > 0 && waitpid(children[child], &status, 0) == children[child]);
        CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }

    check_records();

    // Nothing left behind: every child deleted its files
    CHECK(unlink(SHARED) == 0);
    CHECK(rmdir(DIRECTORY) == 0);
    return test_status();
}
