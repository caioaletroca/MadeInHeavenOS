#include <stdio.h>
#include <FILE.h>

char *fgets(char *s, int n, FILE *f)
{
    if (n <= 0)
        return NULL;

    int i = 0;
    int c = 0;

    // At most n - 1 characters: the last place is for the terminator
    while (i < n - 1)
    {
        c = fgetc(f);
        if (c == EOF)
            break;

        s[i++] = (char)c;

        // The newline is kept: the caller can tell a whole line from a cut one
        if (c == '\n')
            break;
    }

    // End of file before any character, or a read error: no line (C11 7.21.7.2)
    if (c == EOF && (i == 0 || (f->flags & F_ERR)))
        return NULL;

    s[i] = '\0';
    return s;
}