// #include <driver/tty.h>
#include <string.h>
// #include <kernel/kprintf.h>

void kmain() {
    // terminal_clear();
    // kprintf('Test');
    char *VIDEO_MEMORY = (char*) 0xB8000;

    const char *test = "ooinfdaznrlzwrcvmweclqribcgpfvxensyilrygjudlrqwoahmzhenlbogfiayod";
    size_t size = strlen(test);

    *VIDEO_MEMORY = size;
    // terminal_writestring("Hello World!!!");
}