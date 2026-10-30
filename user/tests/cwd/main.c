#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <spawn.h>
#include <string.h>
#include <errno.h>
#include <test.h>

/*
 * The current directory: chdir, relative paths from it (across the /mnt mount
 * both ways), relative creates and removals, the errors that leave it where
 * it was, and a spawned child starting in it. Works on the test disk in /mnt:
 * the same in both boots.
 */

#define SELF "/usr/tests/cwd"
#define HELLO_TEXT "Hello from the disk!\n"

// "." is the directory at path: same inode on the same file system
static int here_is(const char *path)
{
    struct stat here, there;
    return stat(".", &here) == 0 && stat(path, &there) == 0 && here.st_ino == there.st_ino &&
           here.st_dev == there.st_dev;
}

static void test_chdir(void)
{
    CHECK(here_is("/")); // init's, inherited by the runner and by this test

    // Into the test disk: relative names are found there
    CHECK(chdir("/mnt") == 0);
    CHECK(here_is("/mnt"));
    char buffer[32];
    int fd = open("hello.txt", O_RDONLY);
    CHECK(fd >= 0 && read(fd, buffer, sizeof(buffer)) == (ssize_t)strlen(HELLO_TEXT) &&
          memcmp(buffer, HELLO_TEXT, strlen(HELLO_TEXT)) == 0);
    close(fd);

    // A relative chdir, and back up with ".."
    CHECK(chdir("docs") == 0 && here_is("/mnt/docs"));
    struct stat st;
    CHECK(stat("README", &st) == 0 && S_ISREG(st.st_mode));
    CHECK(chdir("deep/nested") == 0 && here_is("/mnt/docs/deep/nested"));
    CHECK(chdir("../..") == 0 && here_is("/mnt/docs"));
    CHECK(chdir("..") == 0 && here_is("/mnt"));

    // ".." from the mounted root steps out of the mount, onto the root's file system
    CHECK(chdir("..") == 0 && here_is("/"));
    CHECK(chdir("mnt/") == 0 && here_is("/mnt")); // a trailing '/' on a directory is fine
    CHECK(chdir(".") == 0 && here_is("/mnt"));
}

static void test_errors(void)
{
    CHECK(chdir("/mnt") == 0);

    // Each refusal leaves the directory where it was
    errno = 0;
    CHECK(chdir("nope") == -1 && errno == ENOENT);
    errno = 0;
    CHECK(chdir("") == -1 && errno == ENOENT);
    errno = 0;
    CHECK(chdir("hello.txt") == -1 && errno == ENOTDIR);
    errno = 0;
    CHECK(chdir("hello.txt/") == -1 && errno == ENOTDIR);
    errno = 0;
    CHECK(chdir("/dev/null") == -1 && errno == ENOTDIR);
    errno = 0;
    CHECK(chdir((const char *)1) == -1 && errno == EFAULT);
    CHECK(here_is("/mnt"));
}

// Creates and removals by bare names land in the current directory
static void test_relative_changes(void)
{
    CHECK(chdir("/mnt") == 0);

    int fd = open("cwd-file", O_WRONLY | O_CREAT | O_EXCL, 0644);
    CHECK(fd >= 0 && write(fd, "cwd", 3) == 3);
    close(fd);
    struct stat st;
    CHECK(stat("/mnt/cwd-file", &st) == 0 && st.st_size == 3);

    CHECK(mkdir("cwd-dir", 0755) == 0);
    CHECK(chdir("cwd-dir") == 0 && here_is("/mnt/cwd-dir"));
    errno = 0;
    CHECK(rmdir(".") == -1 && errno == EINVAL);
    errno = 0;
    CHECK(rmdir("..") == -1 && errno == ENOTEMPTY);
    CHECK(chdir("..") == 0);

    CHECK(rmdir("cwd-dir") == 0);
    CHECK(unlink("cwd-file") == 0);
    errno = 0;
    CHECK(stat("/mnt/cwd-file", &st) == -1 && errno == ENOENT);
    errno = 0;
    CHECK(stat("/mnt/cwd-dir", &st) == -1 && errno == ENOENT);
}

// A child starts in its parent's directory, and its chdir is its own
static void test_inherited(void)
{
    CHECK(chdir("/mnt/docs") == 0);

    char *argv[] = {SELF, "child", NULL};
    pid_t pid;
    int status;
    CHECK(posix_spawn(&pid, SELF, NULL, NULL, argv, environ) == 0);
    CHECK(waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);

    CHECK(here_is("/mnt/docs")); // the child's chdir did not move us
}

// The child: starts in /mnt/docs, then moves away
static int child_main(void)
{
    CHECK(here_is("/mnt/docs"));
    CHECK(stat("README", (struct stat[1]){0}) == 0);
    CHECK(chdir("/") == 0 && here_is("/"));
    return test_status();
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "child") == 0)
        return child_main();

    test_chdir();
    test_errors();
    test_relative_changes();
    test_inherited();
    return test_status();
}
