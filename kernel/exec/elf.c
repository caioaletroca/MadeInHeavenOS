#include <exec/elf.h>
#include <asm/elf.h>
#include <asm/memory.h>
#include <arch/mmu.h>
#include <mm/address_space.h>
#include <stdbool.h>
#include <string.h>
#include <syscall.h>
#include <util.h>

/**
 * Check the ELF header and that the program header table lies inside the image.
 */
static bool elf_header_valid(const elf64_header_t *header, size_t size)
{
    if (size < sizeof(elf64_header_t))
        return false;

    if (memcmp(&header->e_ident[EI_MAG0], ELFMAG, SELFMAG) != 0 ||
        header->e_ident[EI_CLASS] != ELFCLASS64 ||
        header->e_ident[EI_DATA] != ELFDATA2LSB ||
        header->e_ident[EI_VERSION] != EV_CURRENT ||
        header->e_version != EV_CURRENT ||
        header->e_type != ET_EXEC ||
        header->e_machine != ELF_MACHINE)
        return false;

    // Subtraction after the bound check: an addition could wrap and pass
    return header->e_phentsize == sizeof(elf64_program_header_t) &&
           header->e_phoff <= size &&
           header->e_phnum <= (size - header->e_phoff) / sizeof(elf64_program_header_t);
}

int elf_load(address_space_t *space, const void *image, size_t size, uintptr_t *entry, uintptr_t *end)
{
    const elf64_header_t *header = image;

    if (!elf_header_valid(header, size))
        return -ENOEXEC;

    const elf64_program_header_t *phdrs = (const void *)((const uint8_t *)image + header->e_phoff);
    bool entry_ok = false;
    uintptr_t image_end = 0;

    for (uint16_t i = 0; i < header->e_phnum; i++)
    {
        const elf64_program_header_t *ph = &phdrs[i];

        // Empty segments occupy no memory (ld emits them for empty PHDRS)
        if (ph->p_type != PT_LOAD || ph->p_memsz == 0)
            continue;

        // File bytes inside the image; memory inside [USER_BASE, USER_TOP)
        // without wrapping. p_vaddr > USER_TOP must be rejected before
        // USER_TOP - p_vaddr is computed: above USER_TOP (the kernel half) the
        // subtraction wraps to a huge value and any p_memsz would pass.
        if (ph->p_filesz > ph->p_memsz ||
            ph->p_offset > size ||
            ph->p_filesz > size - ph->p_offset ||
            ph->p_vaddr < USER_BASE ||
            ph->p_vaddr > USER_TOP ||
            ph->p_memsz > USER_TOP - ph->p_vaddr)
            return -ENOEXEC;

        // Whole pages are mapped; present pages are always readable, so PF_R needs no flag
        uintptr_t start = ALIGN_DOWN(ph->p_vaddr, PAGE_SIZE);
        uintptr_t segment_end = ALIGN_UP(ph->p_vaddr + ph->p_memsz, PAGE_SIZE);
        unsigned int flags = (ph->p_flags & PF_W ? MMU_WRITE : 0) |
                             (ph->p_flags & PF_X ? MMU_EXEC : 0);

        // Also fails when two segments share a page (already mapped)
        if (address_space_map(space, start, segment_end - start, flags) != 0)
            return -ENOMEM;

        if (segment_end > image_end)
            image_end = segment_end;

        // File bytes go exactly at p_vaddr; the rest up to p_memsz stays zero
        // because mapped frames start zeroed (.bss). Cannot fail: just mapped.
        if (address_space_write(space, ph->p_vaddr, (const uint8_t *)image + ph->p_offset, ph->p_filesz) != 0)
            return -ENOEXEC;

        if ((ph->p_flags & PF_X) &&
            header->e_entry >= ph->p_vaddr &&
            header->e_entry - ph->p_vaddr < ph->p_memsz)
            entry_ok = true;
    }

    if (!entry_ok)
        return -ENOEXEC;

    *entry = header->e_entry;
    *end = image_end; // Set the end of the loaded image
    return 0;
}
