# Multiboot 2 specifications

# Constants
# TODO: Fix the multiboot2 header
#define MAGIC           0xE85250D6      # Magic number for multiboot2
#define PROTECTED_MODE  0               # Specifies GRUB to start at Protected Mode
#define SIZE            0x18            # The size of multiboot2 header, this is constant by definition
#define CHECKSUM        0x17ADAF12      # -(MAGIC + PROTECTED_MODE + SIZE)

#define TYPE            0
#define FLAGS           0
#define TAG_SIZE        0x8

# Header specification
.section .multiboot
header_start:
    .long MAGIC
    .long PROTECTED_MODE
    .long SIZE
    .long CHECKSUM

    # Required end tag for multiboot
    .short TYPE
    .short FLAGS
    .long TAG_SIZE