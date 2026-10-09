#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <test.h>

/*
 * The initial stack as crt0 hands it to main: the kernel's test runner starts
 * every program with argv = {its path} and envp = {"PATH=/bin"}.
 */

// USER_STACK_TOP and USER_STACK_SIZE in the kernel's <asm/memory.h>
#define STACK_TOP 0x00007FFFFFFFF000ULL
#define STACK_SIZE (64 * 1024)

static int on_stack(const void *p)
{
    uintptr_t address = (uintptr_t)p;
    return address >= STACK_TOP - STACK_SIZE && address < STACK_TOP;
}

int main(int argc, char **argv, char **envp)
{
    CHECK(argc == 1);
    CHECK(argv[0] != NULL && strcmp(argv[0], "/usr/tests/args") == 0);
    CHECK(argv[1] == NULL);

    CHECK(envp[0] != NULL && strcmp(envp[0], "PATH=/bin") == 0);
    CHECK(envp[1] == NULL);
    CHECK(environ == envp);

    // The vectors and their strings live on the stack, the strings above the vectors
    CHECK(on_stack(argv) && on_stack(envp));
    CHECK(on_stack(argv[0]) && on_stack(envp[0]));
    CHECK((uintptr_t)argv[0] > (uintptr_t)&envp[1]);

    // crt0 keeps the ABI alignment: GCC assumes it and does not realign the frame
    _Alignas(16) volatile char probe[16];
    CHECK(((uintptr_t)probe & 15) == 0);

    return test_status();
}
