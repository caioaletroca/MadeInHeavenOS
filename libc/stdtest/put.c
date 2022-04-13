#include <stdtest.h>

static int x;
static int y;

void put(char c) {
    int pos = y * 80 + x;
    char *VIDEO_MEMORY = (char*) 0xB8000;
    *(VIDEO_MEMORY + pos * 2) = c;
    x++;
}