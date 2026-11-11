#include <stdio.h>
#include <string.h>
#include <time.h>

/*
 * date [-u] [+FORMAT]: the time now, as strftime formats it. UTC is the only zone MiHOS
 * knows, so -u (UTC) changes nothing and is accepted for scripts that pass it.
 */
int main(int argc, char *argv[])
{
    const char *format = "%a %b %e %H:%M:%S %Z %Y"; // GNU date's default in the C locale

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-u") == 0)
            continue;
        else if (argv[i][0] == '+')
            format = argv[i] + 1;
        else
        {
            fprintf(stderr, "date: unknown argument '%s'\nusage: date [-u] [+FORMAT]\n", argv[i]);
            return 1;
        }
    }

    time_t now = time(NULL);
    struct tm tm;
    gmtime_r(&now, &tm);

    char text[256];

    if (strftime(text, sizeof(text), format, &tm) == 0 && format[0] != '\0')
    {
        fputs("date: the formatted text is too long\n", stderr);
        return 1;
    }

    puts(text);
    return 0;
}