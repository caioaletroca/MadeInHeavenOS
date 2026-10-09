#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *getenv(const char *name)
{
    int length = strlen(name);
    for (char **env = environ; *env != NULL; env++)
    {
        char *entry = *env;
        if (strncmp(entry, name, length) == 0 && entry[length] == '=')
            return entry + length + 1;
    }
    return NULL;
}