// #include <string.h>
#include <kprintf.h>
#include <idt.h>

void kmain() {
    // idt_init();

    kprintf("%s SUAMAE\n", "Test");

    // __asm__ __volatile__("hlt");

    // __asm__ __volatile__ ("int $0x2");

    // char *VIDEO_MEMORY = (char*) 0xB8000;

    // const char *test = "ooinfdaznrlzwrcvmweclqribcgpfvxensyilrygjudlrqwoahmzhenlbogfiayod";
    // size_t size = strlen(test);

    // *VIDEO_MEMORY = size;
}