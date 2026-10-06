#include <fs/file.h>
#include <mm/kmalloc.h>
#include <asm/irq_flags.h>

file_t *file_create(const file_ops_t *ops, unsigned int flags, void *private)
{
    file_t *file = (file_t *)kmalloc(sizeof(file_t));
    if (file == NULL)
        return NULL;

    file->ops = ops;
    file->flags = flags;
    file->refs = 1;
    file->private = private;

    return file;
}

file_t *file_get(file_t *file)
{
    irq_flags_t flags = irq_save();
    if (file != NULL)
        file->refs++;
    irq_restore(flags);
    return file;
}

void file_put(file_t *file)
{
    if (file != NULL)
    {
        irq_flags_t flags = irq_save();

        if (--file->refs == 0)
        {
            // Release the file using its release operation if it exists.
            if (file->ops->release != NULL)
                file->ops->release(file);

            irq_restore(flags);
            kfree(file);
            return;
        }
        irq_restore(flags);
    }
}