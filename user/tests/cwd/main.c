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
 * it was, getcwd (through mounts, the buffer's size, a removed directory),
 * and a spawned child starting in it. Works on the test disk in /mnt: the
 * same in both boots.
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

// getcwd names the current directory: path is where chdir goes, expected what getcwd must give
static int cwd_is(const char *path, const char *expected)
{
    char buffer[128];
    return chdir(path) == 0 && getcwd(buffer, sizeof(buffer)) == buffer && strcmp(buffer, expected) == 0;
}

static void test_getcwd(void)
{
    CHECK(cwd_is("/", "/"));
    CHECK(cwd_is("/mnt", "/mnt")); // a mounted root: named by its mount point
    CHECK(cwd_is("docs/deep/nested", "/mnt/docs/deep/nested"));
    CHECK(cwd_is("../..", "/mnt/docs"));
    CHECK(cwd_is("/usr/tests", "/usr/tests"));
    CHECK(cwd_is("/dev", "/dev"));
    CHECK(cwd_is("//mnt/./docs/../docs/", "/mnt/docs")); // getcwd gives the path, not what chdir got

    // The buffer: exactly the path and its '\0' fits, one byte less is ERANGE, 0 bytes EINVAL
    char buffer[16];
    CHECK(getcwd(buffer, sizeof("/mnt/docs")) == buffer && strcmp(buffer, "/mnt/docs") == 0);
    errno = 0;
    CHECK(getcwd(buffer, sizeof("/mnt/docs") - 1) == NULL && errno == ERANGE);
    errno = 0;
    CHECK(getcwd(buffer, 0) == NULL && errno == EINVAL);
    errno = 0;
    CHECK(getcwd((char *)1, sizeof(buffer)) == NULL && errno == EFAULT);

    // A removed directory has no name any more: ENOENT, though "." still works
    CHECK(mkdir("/mnt/cwd-gone", 0755) == 0 && chdir("/mnt/cwd-gone") == 0);
    CHECK(rmdir("/mnt/cwd-gone") == 0);
    errno = 0;
    CHECK(getcwd(buffer, sizeof(buffer)) == NULL && errno == ENOENT);
    struct stat st;
    CHECK(stat(".", &st) == 0 && S_ISDIR(st.st_mode));
    CHECK(chdir("/") == 0); // the last reference: the directory is freed now (e2fsck checks)
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
    char buffer[32];
    CHECK(getcwd(buffer, sizeof(buffer)) == buffer && strcmp(buffer, "/mnt/docs") == 0);
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
    test_getcwd();
    test_inherited();
    return test_status();
}
