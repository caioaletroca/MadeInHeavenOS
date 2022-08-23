# Multiboot 2 specifications

# Constants
#define MAGIC           0xE85250D6      # Magic number for multiboot2
#define PROTECTED_MODE  0               # Specifies GRUB to start at Protected Mode
# # define ZERO            0x100000000     # Number to handle checksum math

# Header specification
.section .multiboot
header_start:
    .long MAGIC
    # .long PROTECTED_MODE
    # .long header_start - header_end
    # .long PROTECTED_MODE
    # .long header_end - header_start
    # .long ZERO - (MAGIC + PROTECTED_MODE + (header_end - header_start))

    # Required end tag for multiboot
    # .short 0    # type
    # .short 0    # flags
    # .long 8    # tag size
header_end:
