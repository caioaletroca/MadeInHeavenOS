#include <string.h>

char *strtok_r(char *str, const char *delim, char **saveptr)
{
    if (str == NULL)
        str = *saveptr;

    if (str == NULL)
        return NULL;

    str += strspn(str, delim);
    if (*str == '\0')
        return NULL;

    char *token = str;
    str = strpbrk(token, delim);
    if (str)
    {
        *str = '\0';
        *saveptr = str + 1;
    }
    else
    {
        *saveptr = NULL;
    }

    return token;
}