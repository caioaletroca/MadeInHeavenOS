#include <selftest/elf.h>
#include <exec/elf.h>
#include <exec/exec.h>
#include <asm/elf.h>
#include <asm/memory.h>
#include <mm/address_space.h>
#include <mm/kmalloc.h>
#include <addresses.h>
#include <syscall.h>
#include <string.h>
#include <kprintf.h>
#include <panic.h>

#define ELF_TEST_MODULE "hello"

/**
 * One way to break a valid image. `size` replaces the image size when non-zero.
 */
typedef struct
{
    const char *name;
    void (*mutate)(elf64_header_t *header, elf64_program_header_t *text);
    size_t size;
} elf_test_case_t;

static void break_magic(elf64_header_t *h, elf64_program_header_t *t) { (void)t; h->e_ident[EI_MAG0] = 0; }
static void break_class(elf64_header_t *h, elf64_program_header_t *t) { (void)t; h->e_ident[EI_CLASS] = 1; }
static void break_machine(elf64_header_t *h, elf64_program_header_t *t) { (void)t; h->e_machine = 3; }
static void break_type(elf64_header_t *h, elf64_program_header_t *t) { (void)t; h->e_type = 3; }
static void break_nothing(elf64_header_t *h, elf64_program_header_t *t) { (void)h; (void)t; }
static void break_phoff(elf64_header_t *h, elf64_program_header_t *t) { (void)t; h->e_phoff = UINT64_MAX - 8; }
static void break_filesz(elf64_header_t *h, elf64_program_header_t *t) { (void)h; t->p_filesz = t->p_memsz + 1; }
static void break_kernel_half(elf64_header_t *h, elf64_program_header_t *t) { (void)h; t->p_vaddr = KERNEL_VIRTUAL_ADDRESS; }
static void break_wrap(elf64_header_t *h, elf64_program_header_t *t)
{
    (void)h;
    t->p_vaddr = 0xFFFFFFFFFFFFF000;
    t->p_memsz = 0x2000;
}
static void break_entry(elf64_header_t *h, elf64_program_header_t *t) { (void)t; h->e_entry = 0x500000; }

static const elf_test_case_t elf_test_cases[] = {
    {"bad magic", break_magic, 0},
    {"32-bit class", break_class, 0},
    {"wrong machine", break_machine, 0},
    {"ET_DYN", break_type, 0},
    {"truncated image", break_nothing, 32},
    {"program headers out of bounds", break_phoff, 0},
    {"p_filesz > p_memsz", break_filesz, 0},
    {"segment in the kernel half", break_kernel_half, 0},
    {"segment wrapping past 2^64", break_wrap, 0},
    {"entry outside every segment", break_entry, 0},
};

static const boot_module_t *elf_test_module(const boot_info_t *info)
{
    for (size_t i = 0; i < info->module_count; i++)
        if (strcmp(info->modules[i].name, ELF_TEST_MODULE) == 0)
            return &info->modules[i];

    panic("ELF self-test: boot module '%s' not found\n", ELF_TEST_MODULE);
}

/**
 * The first executable PT_LOAD, so cases can break the segment holding the entry.
 */
static elf64_program_header_t *elf_test_text(elf64_header_t *header)
{
    elf64_program_header_t *phdrs = (void *)((uint8_t *)header + header->e_phoff);

    for (uint16_t i = 0; i < header->e_phnum; i++)
        if (phdrs[i].p_type == PT_LOAD && (phdrs[i].p_flags & PF_X))
            return &phdrs[i];

    panic("ELF self-test: no executable segment in '%s'\n", ELF_TEST_MODULE);
}

void elf_selftest(const boot_info_t *info)
{
    const boot_module_t *module = elf_test_module(info);
    const void *image = phys_to_kern(module->start);
    size_t size = module->end - module->start;

    // Valid image: a full exec_load must succeed with the header's entry
    address_space_t *space;
    uintptr_t entry = 0;

    if (exec_load(image, size, &space, &entry) != 0)
        panic("ELF self-test: valid '%s' failed to load\n", ELF_TEST_MODULE);
    if (entry != ((const elf64_header_t *)image)->e_entry)
        panic("ELF self-test: wrong entry %p\n", (void *)entry);

    address_space_destroy(space);

    // Broken copies: each must be rejected, never mapped into the kernel half
    uint8_t *copy = kmalloc(size);
    if (copy == NULL)
        panic("ELF self-test: out of memory\n");

    for (size_t i = 0; i < sizeof(elf_test_cases) / sizeof(elf_test_cases[0]); i++)
    {
        const elf_test_case_t *test = &elf_test_cases[i];
        elf64_header_t *header = (elf64_header_t *)copy;

        memcpy(copy, image, size);
        elf64_program_header_t *text = elf_test_text(header);
        test->mutate(header, text);

        space = address_space_create();
        if (space == NULL)
            panic("ELF self-test: out of memory\n");

        uintptr_t unused = 0;
        int result = elf_load(space, copy, test->size != 0 ? test->size : size, &unused);

        address_space_destroy(space);

        if (result != -ENOEXEC)
            panic("ELF self-test: %s returned %d instead of -ENOEXEC\n", test->name, result);
        if (unused != 0)
            panic("ELF self-test: %s set the entry point on failure\n", test->name);
    }

    kfree(copy);

    kprintf("ELF self-test completed successfully\n");
}
