#include <fs/initrd.h>
#include <fs/vfs.h>
#include <mm/kmalloc.h>
#include <sys/list.h>
#include <mihos/errno.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <panic.h>
#include <util.h>

#define INITRD_MODULE "initrd" // grub.cfg: module2 /boot/initrd.tar initrd
#define TAR_BLOCK 512

static uint64_t initrd_next_ino = 1;
static uint64_t initrd_dev;

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

typedef struct initrd_node
{
    vfs_node_t base; // what the VFS sees (the "base class")
    char *name;      // kmalloc'd copy, '\0'-terminated
    size_t name_length;
    const void *data;  // files: their bytes inside the tar
    list_t children;   // directories: initrd_node_t's, via sibling
    list_t child_link; // link in the parent's children
} initrd_node_t;

// The initrd_node_t around a vfs_node_t (C#: (InitrdNode)node)
#define INITRD_NODE(n) list_container(n, initrd_node_t, base)

static int initrd_lookup(vfs_node_t *dir, const char *name, size_t length, vfs_node_t **child);
static long initrd_read(vfs_node_t *file, void *buffer, size_t count, uint64_t offset);
static int initrd_readdir(vfs_node_t *dir, uint64_t index, vfs_dirent_t *entry);

static const vfs_node_ops_t initrd_ops = {
    .lookup = initrd_lookup,
    .read = initrd_read,
    .readdir = initrd_readdir,
    .release = NULL,
};

/**
 * tar_parse_octal converts a string representing an octal number in a tar header
 * to a size_t value. It skips leading spaces and stops at the first NUL or space.
 * Returns 0 on success, -1 on failure (invalid octal digit).
 */
static int tar_parse_octal(const char *field, size_t length, size_t *value)
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
    if (tar_parse_octal(header->checksum, sizeof(header->checksum), &stored) != 0)
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
    if (tar_parse_octal(header->size, sizeof(header->size), &file_size) != 0)
        return NULL;

    *size = file_size;
    *data = (const char *)header + TAR_BLOCK;
    *offset += TAR_BLOCK + ALIGN_UP(file_size, TAR_BLOCK);

    return header;
}

/**
 * initrd_node_create creates a new initrd node with the given type and name.
 * It allocates memory for the node and its name, initializes the VFS node,
 * and sets up the children and sibling lists.
 * Returns a pointer to the newly created node.
 */
static initrd_node_t *initrd_node_create(vfs_node_type_t type, const char *name, size_t length)
{
    if (length > VFS_NAME_MAX)
        panic("initrd: name '%.*s' is too long\n", (int)length, name);

    initrd_node_t *n = kzalloc(sizeof(*n));
    char *copy = kmalloc(length + 1);
    if (n == NULL || copy == NULL)
        panic("initrd: out of memory building the tree\n");

    vfs_node_init(&n->base, type, &initrd_ops);
    memcpy(copy, name, length);
    copy[length] = '\0';
    n->name = copy;
    n->name_length = length;
    n->base.ino = initrd_next_ino++;
    n->base.dev = initrd_dev;
    list_init(&n->children);
    list_init(&n->child_link);

    return n;
}

/**
 * initrd_find_child searches for a child node with the given name and length
 * within the specified directory node.
 * Returns a pointer to the child node if found, or NULL if not found.
 */
static initrd_node_t *initrd_find_child(initrd_node_t *dir, const char *name, size_t length)
{
    list_t *pos;
    list_for_each(pos, &dir->children)
    {
        initrd_node_t *child = list_container(pos, initrd_node_t, child_link);
        if (child->name_length == length && memcmp(child->name, name, length) == 0)
            return child;
    }

    return NULL;
}

/**
 * initrd_add adds a child node to the specified directory node.
 * It sets the parent of the child node and inserts it into the
 * children list of the directory.
 */
static void initrd_add(initrd_node_t *dir, initrd_node_t *child)
{
    child->base.parent = &dir->base;
    list_insert_before(&dir->children, &child->child_link);
}

/**
 * initrd_find_or_create_dir searches for a child directory with the given name and length
 * within the specified directory node. If the child does not exist, it creates
 * a new directory node and adds it to the parent directory.
 * Returns a pointer to the child directory node.
 */
static initrd_node_t *initrd_find_or_create_dir(initrd_node_t *dir, const char *name, size_t length)
{
    initrd_node_t *child = initrd_find_child(dir, name, length);
    if (child == NULL)
    {
        child = initrd_node_create(VFS_DIRECTORY, name, length);
        child->base.mode = 0755; // Default mode for directories
        initrd_add(dir, child);
    }
    else if (child->base.type != VFS_DIRECTORY)
        panic("initrd: '%.*s' is both a file and a directory\n", (int)length, name);

    return child;
}

/**
 * initrd_lookup searches for a child node with the given name and length
 * within the specified directory node.
 * Returns 0 and sets the child pointer if found, or -ENOENT if not found.
 */
static int initrd_lookup(vfs_node_t *dir, const char *name, size_t length, vfs_node_t **child)
{
    initrd_node_t *found = initrd_find_child(INITRD_NODE(dir), name, length);
    if (found == NULL)
        return -ENOENT;

    vfs_node_get(&found->base);
    *child = &found->base;
    return 0;
}

/**
 * initrd_read reads data from the specified file node into the buffer.
 * It reads up to 'count' bytes starting from the given 'offset'.
 * Returns the number of bytes read, or 0 if the offset is beyond the end of the file.
 */
static long initrd_read(vfs_node_t *file, void *buffer, size_t count, uint64_t offset)
{
    if (offset >= file->size)
        return 0;

    uint64_t available = file->size - offset;
    if (count > available)
        count = (size_t)available;

    memcpy(buffer, (const char *)INITRD_NODE(file)->data + offset, count);
    return (long)count;
}

/**
 * initrd_readdir reads the directory entries of the specified directory node.
 * It fills the provided vfs_dirent_t structure with the entry at the given index.
 * Returns 1 if an entry is found, or 0 if the index is out of bounds.
 */
static int initrd_readdir(vfs_node_t *dir, uint64_t index, vfs_dirent_t *entry)
{
    vfs_node_t *node = NULL;
    const char *name;
    size_t length;

    if (index == 0)
    {
        node = dir;
        name = ".";
        length = 1;
    }
    else if (index == 1)
    {
        node = dir->parent;
        name = "..";
        length = 2;
    }
    else
    {
        uint64_t i = 2;
        list_t *pos;
        list_for_each(pos, &INITRD_NODE(dir)->children)
        {
            if (i++ == index)
            {
                initrd_node_t *child = list_container(pos, initrd_node_t, child_link);

                node = &child->base;
                name = child->name;
                length = child->name_length;
                break;
            }
        }

        if (node == NULL)
            return 0;
    }

    entry->ino = node->ino;
    entry->type = node->type;
    entry->length = length;
    memcpy(entry->name, name, length);
    entry->name[length] = '\0';

    return 1;
}

static void initrd_add_entry(initrd_node_t *root, const tar_header_t *header, size_t size, const void *data)
{
    bool is_file = header->type == '0' || header->type == '\0';
    if (!is_file && header->type != '5')
        return;

    // Full path: "prefix/name", or just name. %.*s stops at a NUL or after the field's size
    char path[sizeof(header->prefix) + 1 + sizeof(header->name) + 1];
    size_t prefix_length = strnlen(header->prefix, sizeof(header->prefix));
    if (prefix_length > 0)
        snprintf(path, sizeof(path), "%.*s/%.*s", (int)prefix_length, header->prefix, (int)sizeof(header->name), header->name);
    else
        snprintf(path, sizeof(path), "%.*s", (int)sizeof(header->name), header->name);

    // Parse the mode and modification time from the tar header.
    size_t mode, mtime;
    if (tar_parse_octal(header->mode, sizeof(header->mode), &mode) != 0 ||
        tar_parse_octal(header->mtime, sizeof(header->mtime), &mtime) != 0)
        panic("initrd: '%s' has a bad mode or mtime\n", path);

    initrd_node_t *dir = root;
    initrd_node_t *last = NULL;
    const char *p = path;
    size_t length;
    const char *name = vfs_path_next(&p, &length);

    while (length > 0)
    {
        // Look ahead: is this the last component?
        size_t next_length;
        const char *next_name = vfs_path_next(&p, &next_length);

        if (next_length > 0 || !is_file)
        {
            dir = initrd_find_or_create_dir(dir, name, length);
            last = dir;
        }
        else
        {
            if (initrd_find_child(dir, name, length) != NULL)
                panic("initrd: '%s' listed twice\n", path);

            initrd_node_t *file = initrd_node_create(VFS_FILE, name, length);
            file->base.size = size;
            file->data = data;
            initrd_add(dir, file);
            last = file;
        }

        name = next_name;
        length = next_length;
    }

    if (last != NULL)
    {
        last->base.mode = (uint32_t)(mode & 07777);
        last->base.mtime = (int64_t)mtime;
    }
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

vfs_node_t *initrd_init(const boot_info_t *boot_info)
{
    const boot_module_t *module = initrd_module(boot_info);
    if (module == NULL)
        return NULL;

    initrd_image = phys_to_kern(module->start);
    initrd_size = module->end - module->start;

    // Validate the tar archive and calculate offsets for each file.
    size_t file_offset = 0;
    while (file_offset + TAR_BLOCK <= initrd_size)
    {
        const tar_header_t *header = (const tar_header_t *)(initrd_image + file_offset);
        if (header->name[0] == '\0')
            break;

        size_t file_size;
        if (!tar_header_valid(header) || tar_parse_octal(header->size, sizeof(header->size), &file_size) != 0)
            panic("initrd: bad header at offset %zu\n", file_offset);

        size_t data = file_offset + TAR_BLOCK;
        if (file_size > initrd_size - data)
            panic("initrd: '%.100s' runs past the end of the archive\n", header->name);

        file_offset = data + ALIGN_UP(file_size, TAR_BLOCK);
    }

    // Allocate a device number for the initrd
    initrd_dev = vfs_dev_alloc();

    // The archive is valid: build the tree and make it "/"
    initrd_node_t *root = initrd_node_create(VFS_DIRECTORY, "", 0);
    root->base.parent = &root->base;
    root->base.mode = 0755; // Default mode for the root directory

    size_t offset = 0;
    size_t size;
    const void *data;
    const tar_header_t *header;
    while ((header = tar_next(&offset, &size, &data)) != NULL)
        initrd_add_entry(root, header, size, data);

    return &root->base;
}