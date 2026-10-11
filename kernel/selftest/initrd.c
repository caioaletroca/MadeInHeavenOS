#include <selftest/initrd.h>
#include <fs/initrd.h>
#include <string.h>
#include <panic.h>

#define ELF_MAGIC "\x7F" "ELF"
#define ELF_HEADER_SIZE 64

/**
 * A program the initrd must hold: found, with an ELF header at its start.
 */
static void initrd_expect_program(const char *path)
{
    size_t size = 0;
    const void *data = initrd_find(path, &size);

    if (data == NULL)
        panic("initrd self-test: '%s' not found\n", path);
    if (size < ELF_HEADER_SIZE || memcmp(data, ELF_MAGIC, 4) != 0)
        panic("initrd self-test: '%s' is not an ELF image (%zu bytes)\n", path, size);
}

/**
 * A path initrd_find must reject, leaving size untouched.
 */
static void initrd_expect_missing(const char *path)
{
    size_t size = 12345;

    if (initrd_find(path, &size) != NULL)
        panic("initrd self-test: '%s' should not be found\n", path);
    if (size != 12345)
        panic("initrd self-test: a failed lookup of '%s' changed the size\n", path);
}

void initrd_selftest(void)
{
    initrd_expect_program("/sbin/init");
    initrd_expect_program("/bin/sh");
    initrd_expect_program("/usr/tests/hello");

    // The size is optional
    if (initrd_find("/sbin/init", NULL) == NULL)
        panic("initrd self-test: lookup without a size failed\n");

    initrd_expect_missing("/nope");
    initrd_expect_missing("/usr/tests");           // a directory, not a file
    initrd_expect_missing("/usr/tests/");          // the directory's own entry
    initrd_expect_missing("usr/tests/hello");      // relative: no current directory yet
    initrd_expect_missing("");
    initrd_expect_missing("/usr/tests/hello/");    // a file is not a directory
    initrd_expect_missing("/usr/tests/hell");      // a prefix of a name
    initrd_expect_missing("/usr/tests/hello.elf"); // the suffix programs lost
    initrd_expect_missing("/sbin/init/x");
}
