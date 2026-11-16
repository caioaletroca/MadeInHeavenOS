#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <test.h>

/*
 * The first program on mlibc (roadmap D9): static, linked against mlibc's
 * libc.a instead of ours, so everything here goes through the MiHOS sysdeps
 * in mlibc/sysdeps/mihos. Each check leans on a different one; reaching main
 * at all means crt1, the entry stack and TcbSet (errno lives in the thread
 * block) worked.
 */

#define SELF_PATH "/usr/tests/mlibc_hello"
#define BLOCK_SIZE (256 * 1024) // past malloc's small slabs: a mapping of its own

static void test_startup(int argc, char **argv)
{
    CHECK(argc == 1);
    CHECK(argv[0] != NULL && strcmp(argv[0], SELF_PATH) == 0);
    CHECK(argv[1] == NULL);
    CHECK(getenv("TEST_ROOT") != NULL); // the runner's environment, read from the stack
}

// Write, Isatty: stdio on the terminal
static void test_output(void)
{
    CHECK(printf("hello from mlibc\n") == 17);
    CHECK(fflush(stdout) == 0);
}

// AnonAllocate, AnonFree: memory comes zeroed and is the program's own
static void test_memory(void)
{
    unsigned char *block = calloc(BLOCK_SIZE, 1);
    CHECK(block != NULL);
    if (block == NULL)
        return;
    CHECK(block[0] == 0 && block[BLOCK_SIZE - 1] == 0);
    memset(block, 0xA5, BLOCK_SIZE);
    CHECK(block[BLOCK_SIZE / 2] == 0xA5);
    free(block);

    char *small = malloc(32);
    CHECK(small != NULL);
    strcpy(small, "small");
    CHECK(strcmp(small, "small") == 0);
    free(small);
}

// Open, Read, Seek, Close: this program's own file starts with the ELF magic
static void test_files(void)
{
    int fd = open(SELF_PATH, O_RDONLY);
    CHECK(fd >= 0);
    if (fd < 0)
        return;
    char magic[4];
    CHECK(read(fd, magic, sizeof(magic)) == 4);
    CHECK(memcmp(magic, "\177ELF", 4) == 0);
    off_t size = lseek(fd, 0, SEEK_END);
    CHECK(size > 4);
    CHECK(lseek(fd, 1, SEEK_SET) == 1);
    CHECK(read(fd, magic, 3) == 3 && memcmp(magic, "ELF", 3) == 0);
    CHECK(close(fd) == 0);

    // A sysdep's error comes back as errno
    errno = 0;
    CHECK(open("/no/such/file", O_RDONLY) == -1 && errno == ENOENT);
    CHECK(close(fd) == -1 && errno == EBADF);
}

// The same through stdio's buffers
static void test_stdio_file(void)
{
    FILE *file = fopen(SELF_PATH, "r");
    CHECK(file != NULL);
    if (file == NULL)
        return;
    CHECK(fgetc(file) == 0x7F);
    CHECK(fgetc(file) == 'E');
    CHECK(fseek(file, 0, SEEK_SET) == 0);
    CHECK(fgetc(file) == 0x7F);
    CHECK(fclose(file) == 0);
}

// ClockGet: the clock moves forward
static void test_clock(void)
{
    struct timespec first, second;
    CHECK(clock_gettime(CLOCK_MONOTONIC, &first) == 0);
    CHECK(clock_gettime(CLOCK_MONOTONIC, &second) == 0);
    CHECK(second.tv_sec > first.tv_sec ||
          (second.tv_sec == first.tv_sec && second.tv_nsec >= first.tv_nsec));
    CHECK(first.tv_nsec >= 0 && first.tv_nsec < 1000000000L);
}

int main(int argc, char **argv)
{
    test_startup(argc, argv);
    test_output();
    test_memory();
    test_files();
    test_stdio_file();
    test_clock();
    return test_status(); // through exit: Exit
}
