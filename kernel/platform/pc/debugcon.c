#include <platform/platform.h>
#include <x86/io.h>

// Bochs (port_e9_hack) and QEMU (-debugcon) copy every byte written here to the host
#define DEBUGCON_PORT 0xE9

void platform_debug_write(const char *s, size_t n)
{
    for (size_t i = 0; i < n; i++)
        outb(DEBUGCON_PORT, (uint8_t)s[i]);
}