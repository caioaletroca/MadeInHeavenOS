#include <fs/initrd.h>
#include <string.h>
#include <stdbool.h>
#include <panic.h>
#include <util.h>

#define INITRD_MODULE "initrd" // grub.cfg: module2 /boot/initrd.tar initrd

#define TAR_BLOCK 512

/**
 * tar_header_t represents the header of a tar archive entry.
 * It follows the USTAR (Unix Standard TAR) format.
 */
typedef struct tar_header
{
    char name[100]; // path; not NUL-terminated if it uses all 100 bytes
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12]; // file size, octal
    char mtime[12];
    char checksum[8]; // sum of the header's bytes (this field counted as spaces), octal
    char type;        // '0' or '\0': regular file, '5': directory, others: links, devices
    char linkname[100];
    char magic[6];   // "ustar\0"
    char version[2]; // "00"
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155]; // long paths: prefix + "/" + name
    char padding[12];
} tar_header_t;

_Static_assert(sizeof(tar_header_t) == TAR_BLOCK, "a tar header is one 512-byte block");

static const unsigned char *initrd_image;
static size_t initrd_size;

/**
 * tar_octal converts a string representing an octal number in a tar header
 * to a size_t value. It skips leading spaces and stops at the first NUL or space.
 * Returns 0 on success, -1 on failure (invalid octal digit).
 */
static int tar_octal(const char *field, size_t length, size_t *value)
{
    size_t result = 0;
    size_t i = 0;

    while (i < length && field[i] == ' ') // Skip leading spaces
        i++;

    for (; i < length && field[i] != '\0' && field[i] != ' '; i++)
    {
        if (field[i] < '0' || field[i] > '7')
            return -1;
        result = result * 8 + (size_t)(field[i] - '0');
    }

    *value = result;
    return 0;
}

/**
 * tar_header_valid checks if a tar header is valid.
 * It verifies the magic field and the checksum.
 * Returns 1 if valid, 0 otherwise.
 */
static int tar_header_valid(const tar_header_t *header)
{
    if (memcmp(header->magic, "ustar", 5) != 0)
        return 0;

    size_t stored;
    if (tar_octal(header->checksum, sizeof(header->checksum), &stored) != 0)
        return 0;

    const unsigned char *bytes = (const unsigned char *)header;
    size_t sum = 0;
    for (size_t i = 0; i < TAR_BLOCK; i++)
    {
        bool in_checksum = i >= offsetof(tar_header_t, checksum) &&
                           i < offsetof(tar_header_t, checksum) + sizeof(header->checksum);
        sum += in_checksum ? ' ' : bytes[i];
    }

    return sum == stored;
}

/**
 * tar_name_equals checks if the name in the tar header matches the given path.
 * It considers the prefix and the name fields.
 * Returns true if they match, false otherwise.
 */
static bool tar_name_equals(const tar_header_t *header, const char *path)
{
    size_t prefix_length = strnlen(header->prefix, sizeof(header->prefix));
    if (prefix_length > 0)
    {
        if (strncmp(header->prefix, path, prefix_length) != 0 || path[prefix_length] != '/')
            return false;

        path += prefix_length + 1; // Skip the prefix and the '/'
    }

    size_t name_length = strnlen(header->name, sizeof(header->name));

    return strlen(path) == name_length && memcmp(path, header->name, name_length) == 0;
}

/**
 * tar_next retrieves the next file in the tar archive.
 * It updates the offset, size, and data pointer.
 * Returns a pointer to the tar header, or NULL if there are no more files.
 */
static const tar_header_t *tar_next(size_t *offset, size_t *size, const void **data)
{
    if (*offset + TAR_BLOCK > initrd_size)
        return NULL;

    const tar_header_t *header = (const tar_header_t *)(initrd_image + *offset);
    if (header->name[0] == '\0')
        return NULL;

    size_t file_size;
    if (tar_octal(header->size, sizeof(header->size), &file_size) != 0)
        return NULL;

    *size = file_size;
    *data = (const char *)header + TAR_BLOCK;
    *offset += TAR_BLOCK + ALIGN_UP(file_size, TAR_BLOCK);

    return header;
}

/**
 * initrd_module finds the boot module corresponding to the initial ramdisk.
 * Returns a pointer to the boot module, or NULL if not found.
 */
static const boot_module_t *initrd_module(const boot_info_t *info)
{
    size_t length = strlen(INITRD_MODULE);

    for (size_t i = 0; i < info->module_count; i++)
    {
        const char *name = info->modules[i].name;
        if (strncmp(name, INITRD_MODULE, length) == 0 && (name[length] == '\0' || name[length] == ' '))
            return &info->modules[i];
    }

    return NULL;
}

void initrd_init(const boot_info_t *boot_info)
{
    const boot_module_t *module = initrd_module(boot_info);
    if (module == NULL)
        panic("initrd: no '%s' boot module\n", INITRD_MODULE);

    initrd_image = phys_to_kern(module->start);
    initrd_size = module->end - module->start;

    size_t file_offset = 0;
    while (file_offset + TAR_BLOCK <= initrd_size)
    {
        const tar_header_t *header = (const tar_header_t *)(initrd_image + file_offset);
        if (header->name[0] == '\0')
            break;

        size_t file_size;
        if (!tar_header_valid(header) || tar_octal(header->size, sizeof(header->size), &file_size) != 0)
            panic("initrd: bad header at offset %zu\n", file_offset);

        size_t data = file_offset + TAR_BLOCK;
        if (file_size > initrd_size - data)
            panic("initrd: '%.100s' runs past the end of the archive\n", header->name);

        file_offset = data + ALIGN_UP(file_size, TAR_BLOCK);
    }
}

const void *initrd_find(const char *path, size_t *size)
{
    const char *relative_path = path;

    if (path[0] != '/')
        return NULL;
    relative_path++; // Skip the leading '/'

    size_t file_offset = 0;
    size_t file_size = 0;
    const void *data = NULL;
    const tar_header_t *header;
    while ((header = tar_next(&file_offset, &file_size, &data)) != NULL)
    {
        // Only regular files: '0', or '\0' from old tar versions
        if (header->type != '0' && header->type != '\0')
            continue;

        if (tar_name_equals(header, relative_path))
        {
            if (size)
                *size = file_size;
            return data;
        }
    }

    return NULL;
}