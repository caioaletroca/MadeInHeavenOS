#include <platform/platform.h>
#include <x86/io.h>

#define QEMU_Q35_PM1A_CTRL 0x604 // q35 (ICH9): SeaBIOS puts the ACPI PM base at 0x600
#define QEMU_PC_PM1A_CTRL 0xB004 // pc (PIIX4), QEMU's default: PM base 0xB000
#define ACPI_SLP_EN_S5 0x2000    // SLP_EN with sleep type 0, which is S5 (soft off) on both

#define BOCHS_SHUTDOWN_PORT 0x8900

void platform_power_off(void)
{
    // QEMU (pc and q35): ACPI PM1a control, sleep type S5 + enable
    outw(QEMU_Q35_PM1A_CTRL, ACPI_SLP_EN_S5);
    outw(QEMU_PC_PM1A_CTRL, ACPI_SLP_EN_S5);

    for (const char *c = "Shutdown"; *c; c++)
        outb(BOCHS_SHUTDOWN_PORT, *c);

    // still running: real hardware, the caller halts
}
