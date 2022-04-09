#include <kernel/tty.h>
#include <string.h>

void kmain() {
    terminal_clear();
    terminal_writestring("Hello World!!!");
}