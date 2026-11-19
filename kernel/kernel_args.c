#include <kernel_args.h>
#include <string.h>
#include <logger.h>
#include <panic.h>

/*
 * One word of the command line, pointing into the line itself (nothing is
 * copied): the name, and the value after '=' if there is one. Neither is
 * '\0'-terminated where the word ends, so they always go with their lengths.
 */
typedef struct kernel_arg_word
{
    const char *name;
    size_t name_length;
    const char *value; // NULL for a bare name ("quiet")
    size_t value_length;
} kernel_arg_word_t;

// What a parameter does with its value; false = a value it cannot take
typedef bool (*kernel_arg_set_t)(kernel_args_t *args, const kernel_arg_word_t *word);

// One parameter the kernel knows: its name, its kind, and what sets it
typedef struct kernel_arg_spec
{
    const char *name;
    bool is_switch; // on/off: may appear without a value (Linux's bool parameters)
    kernel_arg_set_t set;
} kernel_arg_spec_t;

// Whether the word's value is exactly text: same length, same bytes ("only" is not "on")
static bool word_value_is(const kernel_arg_word_t *word, const char *text)
{
    size_t length = strlen(text);
    return word->value_length == length && strncmp(word->value, text, length) == 0;
}

/**
 * @brief Reads an on/off value: none (the bare name), 1, on or yes is on;
 * 0, off or no is off.
 *
 * @param word The switch's word.
 * @param out Receives the value; left as it was for any other value.
 * @return false when the value is none of those.
 */
static bool switch_parse(const kernel_arg_word_t *word, bool *out)
{
    if (word->value == NULL || word_value_is(word, "1") || word_value_is(word, "on") || word_value_is(word, "yes"))
        *out = true;
    else if (word_value_is(word, "0") || word_value_is(word, "off") || word_value_is(word, "no"))
        *out = false;
    else
        return false;

    return true;
}

// selftests[=on|off]: run the kernel's self-tests before init
static bool selftests_set(kernel_args_t *args, const kernel_arg_word_t *word)
{
    return switch_parse(word, &args->selftests);
}

/*
 * root=<device>: the block device holding '/'. A name too long for a device
 * panics: ignoring it would boot the initrd instead of the disk asked for.
 */
static bool root_set(kernel_args_t *args, const kernel_arg_word_t *word)
{
    if (word->value_length >= sizeof(args->root))
        panic("kernel_args: root=%.*s is too long\n", (int)word->value_length, word->value);

    memcpy(args->root, word->value, word->value_length);
    args->root[word->value_length] = '\0';
    return true;
}

// quiet: only errors and worse on the console; quiet=0 puts the default back
static bool quiet_set(kernel_args_t *args, const kernel_arg_word_t *word)
{
    bool quiet;
    if (!switch_parse(word, &quiet))
        return false;

    args->console_level = quiet ? LOGGER_CONSOLE_QUIET : LOGGER_CONSOLE_DEFAULT;
    return true;
}

static bool loglevel_set(kernel_args_t *args, const kernel_arg_word_t *word)
{
    if (word->value_length != 1 || word->value[0] < '0' || word->value[0] > '0' + LOGGER_CONSOLE_MAX)
        return false;

    args->console_level = word->value[0] - '0';
    return true;
}

// Every parameter the kernel knows; a new one is a setter plus a line here
static const kernel_arg_spec_t kernel_arg_specs[] = {
    {"root", false, root_set},
    {"selftests", true, selftests_set},
    {"quiet", true, quiet_set},
    {"loglevel", false, loglevel_set},
};

/**
 * @brief Reads the word at *cursor and moves *cursor past it.
 *
 * Words are separated by spaces; a name ends at the first '=' in its word.
 *
 * @param cursor Where to read; left after the word (or at the line's end).
 * @param word Receives the word, pointing into the line.
 * @return false when only spaces are left.
 */
static bool word_next(const char **cursor, kernel_arg_word_t *word)
{
    const char *p = *cursor;
    while (*p == ' ')
        p++;

    size_t length = strcspn(p, " ");
    if (length == 0)
    {
        *cursor = p;
        return false;
    }

    size_t name_length = strcspn(p, "= "); // stops at '=', or at the word's end
    word->name = p;
    word->name_length = name_length;

    if (name_length < length) // stopped at '=': a value follows
    {
        word->value = p + name_length + 1;
        word->value_length = length - name_length - 1;
    }
    else
    {
        word->value = NULL;
        word->value_length = 0;
    }

    *cursor = p + length;
    return true;
}

// The parameter with the word's name, or NULL when the kernel does not know it
static const kernel_arg_spec_t *spec_find(const kernel_arg_word_t *word)
{
    for (size_t i = 0; i < sizeof(kernel_arg_specs) / sizeof(kernel_arg_specs[0]); i++)
    {
        const kernel_arg_spec_t *spec = &kernel_arg_specs[i];
        if (strlen(spec->name) == word->name_length && strncmp(word->name, spec->name, word->name_length) == 0)
            return spec;
    }
    return NULL;
}

void kernel_args_parse(const char *cmdline, kernel_args_t *args)
{
    args->root[0] = '\0';
    args->selftests = false;
    args->console_level = LOGGER_CONSOLE_DEFAULT;

    const char *p = cmdline;
    kernel_arg_word_t word;

    while (word_next(&p, &word))
    {
        // "--" ends the kernel's words: what follows is init's, even a name the kernel knows
        if (word.value == NULL && word.name_length == 2 && strncmp(word.name, "--", 2) == 0)
            break;

        const kernel_arg_spec_t *spec = spec_find(&word);
        if (spec == NULL)
        {
            if (word.value == NULL)
            {
                p = word.name; // a bare word the kernel does not know: init's first word, given back
                break;
            }
            log_warning("kernel: unknown parameter '%.*s', ignored\n", (int)word.name_length, word.name);
        }
        else if (word.value == NULL && !spec->is_switch)
            log_warning("kernel: %s needs a value, ignored\n", spec->name);
        else if (!spec->set(args, &word))
            log_warning("kernel: bad value '%s=%.*s', ignored\n", spec->name, (int)word.value_length, word.value);
    }

    while (*p == ' ')
        p++;

    args->init_args = p;
}