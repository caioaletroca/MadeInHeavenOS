#include <x86/tss.h>
#include <string.h>
#include <x86/gdt.h>
#include <asm/memory.h>

#define TSS_IST_STACK_SIZE PAGE_SIZE

// Access byte of a 64-bit TSS descriptor: present, type 0x9 (available TSS)
#define GDT_TSS_ACCESS (0x80 | 0x09) // Present bit | type (Available 64-bit TSS)

/**
 * @brief 16-byte system descriptor: a normal descriptor plus the upper
 * 32 bits of the base.
 */
typedef struct gdt_tss_descriptor
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t limit_high_flags;
    uint8_t base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed)) gdt_tss_descriptor_t;

extern uint8_t gdt64[];

static tss_t tss;
static uint8_t ist_stacks[2][TSS_IST_STACK_SIZE]
    __attribute__((aligned(16)));

void tss_init(void)
{
    memset(&tss, 0, sizeof(tss));

    // Stacks grow down: the TSS holds their top
    tss.ist[TSS_IST_NMI - 1] = (uintptr_t)&ist_stacks[0] + TSS_IST_STACK_SIZE;
    tss.ist[TSS_IST_DOUBLE_FAULT - 1] = (uintptr_t)&ist_stacks[1] + TSS_IST_STACK_SIZE;

    // No I/O bitmap: with IOPL 0, ring 3 cannot use in/out at all
    tss.iomap_base = sizeof(tss);

    uintptr_t base = (uintptr_t)&tss;
    uint32_t limit = sizeof(tss) - 1;

    gdt_tss_descriptor_t *descriptor = (gdt_tss_descriptor_t *)(gdt64 + GDT_INDEX_TSS * GDT_ENTRY_SIZE);
    descriptor->limit_low = limit & 0xFFFF;
    descriptor->base_low = base & 0xFFFF;
    descriptor->base_middle = (base >> 16) & 0xFF;
    descriptor->access = GDT_TSS_ACCESS;
    descriptor->limit_high_flags = (limit >> 16) & 0x0F;
    descriptor->base_high = (base >> 24) & 0xFF;
    descriptor->base_upper = base >> 32;
    descriptor->reserved = 0;

    // Load the task register. The CPU marks the descriptor busy (type 0xB),
    // so this must run exactly once.
    __asm__ __volatile__("ltr %w0" : : "r"((uint16_t)TSS_SELECTOR) : "memory");
}

void tss_set_kernel_stack(uintptr_t top)
{
    tss.rsp[0] = top;
}