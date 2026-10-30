#include <kernel_args.h>
#include <string.h>
#include <kprintf.h>
#include <panic.h>

void kernel_args_parse(const char *cmdline, kernel_args_t *args)
{
    args->root[0] = '\0';
    const char *p = cmdline;

    for (;;)
    {
        while (*p == ' ')
            p++;

        size_t length = strcspn(p, " ");
        size_t name_length = strcspn(p, "= ");

        if (length == 0 || p[name_length] != '=')
            break;

        const char *value = p + name_length + 1;
        size_t value_length = length - name_length - 1;

        if (name_length == 4 && strncmp(p, "root", 4) == 0)
        {
            if (value_length >= sizeof(args->root))
                panic("kernel_args: root=%.*s is too long\n", (int)value_length, value);

            strlcpy(args->root, value, value_length + 1); // + 1: strlcpy takes the size with the '\0'
        }
        else
            kprintf("kernel: unknown parameter '%.*s', ignored\n", (int)name_length, p);

        p += length;
    }

    args->init_args = p;
}