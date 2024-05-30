#include <kmalloc.h>

void *kmalloc(size_t size) {
    void *ptr;
    extern char _kernel_physical_end;
    static char *kbrk = NULL;

    if(kbrk == NULL) {
        kbrk = (char *)ALIGN_UP((uintptr_t)&_kernel_physical_end, sizeof(uintptr_t));
    }
    ptr = kbrk;
    kbrk += ALIGN_UP(size, sizeof(uintptr_t));
    return ptr;
}