#include <driver/mem.h>
#include <fs/file.h>
#include <fs/devfs.h>
#include <mihos/stat.h>
#include <string.h>
#include <panic.h>

#define MEM_MAJOR 1
#define NULL_MINOR 3
#define ZERO_MINOR 5

/**
 * @brief Read from the null device. Always returns 0.
 */
static long null_read(file_t *file, char *buf, size_t count)
{
    (void)file;
    (void)buf;
    (void)count;
    return 0;
}

/**
 * @brief Write to the null device. Always returns count
 */
static long null_write(file_t *file, const char *buf, size_t count)
{
    (void)file;
    (void)buf;
    return (long)count;
}

static long zero_read(file_t *file, char *buf, size_t count)
{
    (void)file;
    memset(buf, 0, count);
    return (long)count;
}

static const file_ops_t null_ops = {
    .read = null_read,
    .write = null_write,
    .release = NULL,
};

static const file_ops_t zero_ops = {
    .read = zero_read,
    .write = null_write,
    .release = NULL,
};

void mem_devices_init(void)
{
    int ret = devfs_register("null", 0666, MIHOS_MKDEV(MEM_MAJOR, NULL_MINOR), &null_ops);
    if (ret == 0)
        ret = devfs_register("zero", 0666, MIHOS_MKDEV(MEM_MAJOR, ZERO_MINOR), &zero_ops);

    if (ret < 0)
        panic("mem: cannot register /dev/null and /dev/zero: %d\n", ret);
}