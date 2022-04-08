#include <kernel/tty.h>
#include <string.h>

void kmain() {
    terminal_clear();

    // Create a pointer to a char , and point it to the first text cell of
    // video memory (i.e. the top - left of the screen )
    char * video_memory = (char*) 0xb8000;
    // At the address pointed to by video_memory , store the character ’X’
    // (i.e. display ’X’ in the top - left of the screen ).

    char str1[]="Sample string";
    char str2[40];
    strcpy(str2,str1);

    *video_memory = str2[0];
}