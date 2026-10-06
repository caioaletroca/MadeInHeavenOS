#include <fs/file.h>
#include <mm/kmalloc.h>

file_t *file_create(const file_ops_t *ops, unsigned int flags, void *private)
{
    file_t *file = (file_t *)kmalloc(sizeof(file_t));
    if (file == NULL)
        return NULL;

    file->ops = ops;
    file->flags = flags;
    file->refs = 1;
    file->private = private;
    spinlock_init(&file->lock);

    return file;
}

file_t *file_get(file_t *file)
{
    if (file == NULL)
        return NULL;

    guard(spinlock, &file->lock);
    file->refs++;
    return file;
}

void file_put(file_t *file)
{
    bool last = false;

    if (file == NULL)
        return;

    scoped_guard(spinlock, &file->lock)
    {
        last = (--file->refs == 0);
    }

    if (!last)
        return;

    // Release the file using its release operation if it exists.
    if (file->ops->release != NULL)
        file->ops->release(file);

    kfree(file);
}