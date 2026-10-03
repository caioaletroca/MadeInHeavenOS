#include "selftest/user.h"
#include <addresses.h>
#include <asm/memory.h>
#include <arch/mmu.h>
#include <driver/timer.h>
#include <mm/address_space.h>
#include <sched/thread.h>
#include <kprintf.h>
#include <panic.h>

#define USER_COUNTER_OFFSET 0x100

// loop: incq counter(%rip); jmp loop   (counter at USER_BASE + 0x100)
static const uint8_t user_loop[] = {
    0x48,
    0xFF,
    0x05,
    0xF9,
    0x00,
    0x00,
    0x00, // incq 0xF9(%rip)
    0xEB,
    0xF7, // jmp -9
};

/**
 * Read the user program's counter through the kernel's direct map.
 */
static uint64_t user_counter_read(address_space_t *space)
{
    physaddr_t physical;

    if (arch_mmu_translate(space->root, USER_BASE + USER_COUNTER_OFFSET, &physical) != 0)
        panic("User counter page is not mapped\n");

    return *(volatile uint64_t *)phys_to_kern(physical);
}

void user_selftest(void)
{
    address_space_t *space = address_space_create();
    if (space == NULL)
        panic("Failed to create user test address space\n");

    // One page holding code and counter (writable; executable since NX is off),
    // one page of user stack
    if (address_space_map(space, USER_BASE, PAGE_SIZE, MMU_WRITE | MMU_EXEC) != 0 ||
        address_space_map(space, USER_STACK_TOP - PAGE_SIZE, PAGE_SIZE, MMU_WRITE) != 0)
        panic("Failed to map user test pages\n");

    if (address_space_write(space, USER_BASE, user_loop, sizeof(user_loop)) != 0)
        panic("Failed to load user test program\n");

    if (thread_create_user(space, USER_BASE, USER_STACK_TOP) == NULL)
        panic("Failed to create user test thread\n");

    // The loop never ends (no exit syscall yet): watch it count
    uint64_t previous = 0;

    for (int i = 0; i < 3; i++)
    {
        thread_sleep(TIMER_FREQUENCY_HZ);

        uint64_t counter = user_counter_read(space);
        kprintf("User mode counter: %u\n", (unsigned int)counter);

        if (counter <= previous)
            panic("User thread is not making progress\n");

        previous = counter;
    }

    kprintf("User mode self-test completed successfully\n");
}