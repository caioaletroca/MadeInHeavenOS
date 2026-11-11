#include "FILE.h"

static unsigned char stdin_buffer[BUFSIZ];
static unsigned char stdout_buffer[BUFSIZ];
static unsigned char stderr_buffer[BUFSIZ];

static FILE stdio_streams[3] = {
    {.fd = 0,
     .flags = F_READ | F_LINEBUF | F_STATIC,
     .mode = MODE_NONE,
     .buffer = stdin_buffer,
     .size = BUFSIZ,
     .pos = 0,
     .len = 0,
     .own_buffer = stdin_buffer,
     .ungot = EOF,
     .next = &stdio_streams[1]},
    {.fd = 1,
     .flags = F_WRITE | F_LINEBUF | F_TTYCHECK | F_STATIC,
     .mode = MODE_NONE,
     .buffer = stdout_buffer,
     .size = BUFSIZ,
     .pos = 0,
     .len = 0,
     .own_buffer = stdout_buffer,
     .ungot = EOF,
     .next = &stdio_streams[2]},
    {.fd = 2,
     .flags = F_WRITE | F_NOBUF | F_STATIC,
     .mode = MODE_NONE,
     .pos = 0,
     .len = 0,
     .buffer = stderr_buffer,
     .own_buffer = stderr_buffer,
     .ungot = EOF,
     .next = NULL}};

FILE *__stdio_head = &stdio_streams[0];

FILE *const stdin = &stdio_streams[0];
FILE *const stdout = &stdio_streams[1];
FILE *const stderr = &stdio_streams[2];