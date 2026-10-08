#ifndef _EXEC_ELF_H_
#define _EXEC_ELF_H_

#include <stdint.h>
#include <stddef.h>
#include <arch/mmu.h>

#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define EV_CURRENT 1
#define ET_EXEC 2 /* Executable file */
#define PT_LOAD 1 /* Loadable program segment */
#define PF_X 1    /* Executable */
#define PF_W 2    /* Writable */
#define PF_R 4    /* Readable */

#define EI_MAG0 0 // e_ident[0..3] = 0x7F 'E' 'L' 'F'
#define EI_CLASS 4
#define EI_DATA 5
#define EI_VERSION 6
#define ELFMAG "\x7F" \
               "ELF"
#define SELFMAG 4

/**
 * @file elf.h
 * @brief Definitions for ELF64 headers and program headers.
 */
typedef struct
{
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} elf64_header_t;

/**
 * @brief ELF64 program header structure.
 */
typedef struct elf64_program_header
{
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} elf64_program_header_t;

_Static_assert(sizeof(elf64_header_t) == 64, "ELF64 header must be 64 bytes");
_Static_assert(sizeof(elf64_program_header_t) == 56, "ELF64 program header must be 56 bytes");

struct address_space;

/**
 * @brief Load an ELF64 image into the given address space.
 *
 * Validates the image and maps every PT_LOAD segment into the user half,
 * copying its file bytes; the rest of each segment (.bss) stays zero. Does not
 * create a process. On error the space may already hold some segments: the
 * caller destroys it.
 *
 * @param space The address space to load the ELF image into.
 * @param image Pointer to the ELF image in memory.
 * @param size Size of the ELF image in bytes.
 * @param entry Set to the entry point on success, untouched on failure.
 * @param end Set to the end of the loaded image on success, untouched on failure.
 * @return 0 on success, -ENOEXEC for an invalid image, -ENOMEM if mapping fails.
 */
int elf_load(struct address_space *space, const void *image, size_t size, uintptr_t *entry, uintptr_t *end);

#endif /* _EXEC_ELF_H_ */