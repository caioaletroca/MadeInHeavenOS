#include <selftest/kernel_args.h>
#include <kernel_args.h>
#include <string.h>
#include <kprintf.h>
#include <panic.h>

/**
 * One command line and what the parser must make of it. The kernel's words
 * are the leading ones: name=value, or the bare name of a switch. The first
 * other bare word starts init's arguments; "--" ends the kernel's words.
 */
typedef struct kernel_args_case
{
    const char *line;
    const char *root;      // root= ("" = the initrd)
    bool selftests;
    const char *init_args; // init's words, exactly
} kernel_args_case_t;

static const kernel_args_case_t cases[] = {
    // Nothing, or init's words only
    {"", "", false, ""},
    {"once /usr/tests/run", "", false, "once /usr/tests/run"},

    // GRUB's entries
    {"root=hdb1 respawn /bin/sh", "hdb1", false, "respawn /bin/sh"},
    {"selftests=1 once /usr/tests/run", "", true, "once /usr/tests/run"},
    {"root=hdb1 selftests=1 once /usr/tests/run disk", "hdb1", true, "once /usr/tests/run disk"},

    // A switch: bare is on, and every spelling of on and off
    {"selftests once x", "", true, "once x"},
    {"selftests=0 once x", "", false, "once x"},
    {"selftests=on once x", "", true, "once x"},
    {"selftests=yes once x", "", true, "once x"},
    {"selftests=off once x", "", false, "once x"},
    {"selftests=no once x", "", false, "once x"},
    {"selftests=1 selftests=0 once x", "", false, "once x"}, // the last one wins

    // Spaces: any number between words; init's own words kept as written
    {"  root=hda   selftests   once  x ", "hda", true, "once  x "},

    // Only kernel words, ending the line
    {"root=hdb1", "hdb1", false, ""},
    {"selftests ", "", true, ""},

    // "--" ends the kernel's words, even before a name the kernel knows
    {"-- selftests", "", false, "selftests"},
    {"selftests -- root=hdb1 x", "", true, "root=hdb1 x"},
    {"--", "", false, ""},

    // Mistakes: warned about and skipped, the rest still parsed
    {"unknown=1 once x", "", false, "once x"},
    {"selftests=maybe once x", "", false, "once x"},
    {"root once x", "", false, "once x"}, // root needs a value
};

void kernel_args_selftest(void)
{
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        const kernel_args_case_t *c = &cases[i];
        kernel_args_t args;

        // Garbage first: the parser must set every field itself
        memset(&args, 0x5A, sizeof(args));
        kernel_args_parse(c->line, &args);

        if (strcmp(args.root, c->root) != 0)
            panic("kernel_args self-test: \"%s\": root \"%s\", expected \"%s\"\n", c->line, args.root, c->root);
        if (args.selftests != c->selftests)
            panic("kernel_args self-test: \"%s\": selftests %d, expected %d\n", c->line, args.selftests, c->selftests);
        if (args.init_args == NULL || strcmp(args.init_args, c->init_args) != 0)
            panic("kernel_args self-test: \"%s\": init's words \"%s\", expected \"%s\"\n", c->line,
                  args.init_args == NULL ? "(null)" : args.init_args, c->init_args);
    }
}
