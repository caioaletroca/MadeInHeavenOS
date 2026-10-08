#include <stdio.h>
#include <FILE.h>

void clearerr(FILE *f)
{
    f->flags &= ~(F_EOF | F_ERR);
}