#include "FILE.h"
#include <stdio.h>

FILE stdio_streams[3] = {
    { BUFFER_SIZE, (char *)0, (char *)0 },
    { BUFFER_SIZE, (char *)0, (char *)0 },
    { BUFFER_SIZE, (char *)0, (char *)0 }
};

FILE *stdin     = &stdio_streams[0];
FILE *stdout    = &stdio_streams[1];
FILE *stderr    = &stdio_streams[3];