#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define EXIT_BAD_ARGUMENTS 2

#define BASE_PATH "/"

/**
 * Lists the contents of a directory.
 *
 * @param path The path to the directory.
 * @param show_all Whether to show hidden files.
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on failure.
 */
static int list(const char *path, int show_all)
{
    DIR *dir = opendir(path);
    if (dir == NULL)
    {
        fprintf(stderr, "ls: %s: %s\n", path, strerror(errno));
        return EXIT_FAILURE;
    }

    errno = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (!show_all && entry->d_name[0] == '.')
            continue;

        printf("%s%s\n", entry->d_name, entry->d_type == DT_DIR ? "/" : "");
    }

    if (errno != 0)
    {
        fprintf(stderr, "ls: %s: %s\n", path, strerror(errno));
        closedir(dir);
        return EXIT_FAILURE;
    }

    closedir(dir);
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
    int show_all = 0;

    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1] != '\0'; i++)
    {
        if (strcmp(argv[i], "--") == 0)
        {
            i++;
            break;
        }

        for (const char *flag = argv[i] + 1; *flag != '\0'; flag++)
        {
            if (*flag == 'a')
                show_all = 1;
            else
            {
                fprintf(stderr, "ls: unknown option -%c\nusage: ls [-a] [path...]\n", *flag);
                return EXIT_BAD_ARGUMENTS;
            }
        }
    }

    if (i == argc)
        return list(BASE_PATH, show_all);

    int several = argc - i > 1;
    int status = EXIT_SUCCESS;
    for (int first = i; i < argc; i++)
    {
        if (several)
            printf("%s%s:\n", i > first ? "\n" : "", argv[i]);
        status |= list(argv[i], show_all);
    }

    return status;
}