#include <exec/exec.h>
#include <exec/elf.h>
#include <asm/memory.h>
#include <mm/address_space.h>
#include <mm/kmalloc.h>
#include <syscall.h>
#include <string.h>
#include <util.h>

// Maximum size of the argument and environment strings on the stack.
#define ARGS_MAX (USER_STACK_SIZE / 4)

/**
 * Count the number of elements in a NULL-terminated vector.
 */
static size_t vector_count(char *const vector[])
{
    size_t count = 0;
    while (vector[count] != NULL)
        count++;
    return count;
}

/**
 * Write a NULL-terminated vector of strings into the given address space.
 * Updates the address and records the addresses of each string in the slots array.
 * Returns 0 on success, -1 on failure.
 */
static int strings_write(address_space_t *space, char *const vector[], uintptr_t *address, uint64_t *slots)
{
    size_t i = 0;
    for (; vector[i] != NULL; i++)
    {
        size_t length = strlen(vector[i]) + 1;
        if (address_space_write(space, *address, vector[i], length) != 0)
            return -1;
        slots[i] = *address;
        *address += length;
    }
    slots[i] = 0;
    return 0;
}

/**
 * Build the initial stack for a new process.
 * Returns 0 on success, -E2BIG if the arguments are too large, -ENOMEM on memory allocation failure,
 * or -EFAULT if writing to the address space fails.
 */
static int stack_build(address_space_t *space, char *const argv[], char *const envp[], uintptr_t *stack)
{
    size_t argc = vector_count(argv);
    size_t envc = vector_count(envp);

    size_t string_bytes = 0;
    for (size_t i = 0; i < argc; i++)
        string_bytes += strlen(argv[i]) + 1;
    for (size_t i = 0; i < envc; i++)
        string_bytes += strlen(envp[i]) + 1;

    size_t words = 1 + argc + 1 + envc + 1 + 2;
    if (string_bytes + words * sizeof(uint64_t) > ARGS_MAX)
        return -E2BIG;

    uintptr_t strings = USER_STACK_TOP - string_bytes;
    uintptr_t sp = ALIGN_DOWN(strings - words * sizeof(uint64_t), 16);

    uint64_t *vector = kmalloc(words * sizeof(uint64_t));
    if (vector == NULL)
        return -ENOMEM;

    vector[0] = argc;
    uintptr_t address = strings;
    int ret = strings_write(space, argv, &address, &vector[1]);
    if (ret == 0)
        ret = strings_write(space, envp, &address, &vector[1 + argc + 1]);

    vector[words - 2] = 0;
    vector[words - 1] = 0;

    if (ret == 0)
        ret = address_space_write(space, sp, vector, words * sizeof(uint64_t));

    kfree(vector);
    if (ret != 0)
        return -EFAULT;

    *stack = sp;
    return 0;
}

int exec_load(const void *image,
              size_t size,
              char *const argv[],
              char *const envp[],
              address_space_t **space,
              uintptr_t *entry,
              uintptr_t *stack)
{
    *space = address_space_create();
    if (*space == NULL)
        return -ENOMEM;

    uintptr_t end;
    int ret = elf_load(*space, image, size, entry, &end);

    if (ret == 0 && address_space_map(*space, USER_STACK_TOP - USER_STACK_SIZE, USER_STACK_SIZE, MMU_WRITE) != 0)
        ret = -ENOMEM;

    if (ret == 0)
        ret = stack_build(*space, argv, envp, stack);

    if (ret != 0)
    {
        address_space_destroy(*space);
        *space = NULL;
        return ret;
    }

    (*space)->heap_start = (*space)->brk = ALIGN_UP(end, PAGE_SIZE);

    return ret;
}