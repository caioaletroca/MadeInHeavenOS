#include <x86/paging.h>
#include <x86/isr.h>
#include <x86/vectors.h>
#include <mm/frame.h>
#include <mm/kmalloc.h>
#include <sched/scheduler.h>
#include <sched/process.h>
#include <mihos/signal.h>
#include <kprintf.h>
#include <panic.h>
#include <string.h>

extern page_table_t page_table_l4;

#define fault_address_get(virt) \
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(virt));

#define P1_INDEX_SHIFT 12
#define P2_INDEX_SHIFT 21
#define P3_INDEX_SHIFT 30
#define P4_INDEX_SHIFT 39

#define PAGE_ENTRY_INDEX(address, shift) \
    (((uintptr_t)(address) >> (shift)) & 0x1FFULL)

#define PAGE_4K_SIZE 0x1000ULL
#define PAGE_2M_SIZE 0x200000ULL
#define PAGE_4K_ADDRESS_MASK 0x000FFFFFFFFFF000ULL
#define PAGE_2M_ADDRESS_MASK 0x000FFFFFFFE00000ULL

/**
 * Retrieves the page table at the given index from the specified page table.
 *
 * @param pg The page table to retrieve the entry from.
 * @param index The index of the entry.
 * @return A pointer to the page table at the specified index.
 */
static inline page_table_t *page_table_from_entry(page_table_entry_t entry)
{
    physaddr_t physical = entry & PAGE_4K_ADDRESS_MASK;
    return phys_to_kern(physical);
}

/**
 * Creates a new page table and maps it to the specified physical address.
 *
 * @param physical The physical address to map the new page table to.
 * @return A pointer to the newly created page table, or NULL on failure.
 */
static page_table_t *page_table_create(physaddr_t *physical_out)
{
    physaddr_t physical = frame_alloc(0, 0);

    if (physical == 0)
        return NULL;

    page_table_t *table = phys_to_kern(physical);
    if (table == NULL)
    {
        frame_free(physical, 0);
        return NULL;
    }

    memset(table, 0, PAGE_SIZE);

    *physical_out = physical;
    return table;
}

/**
 * Retrieves the next level page table from the given parent page table at the specified index.
 * If the next level page table does not exist, it will be created and mapped with the provided flags.
 *
 * @param parent The parent page table.
 * @param index The index of the entry in the parent page table.
 * @param flags The flags to use when creating a new page table if necessary.
 * @return A pointer to the next level page table, or NULL on failure.
 */
static page_table_t *page_table_next(page_table_t *parent, uint16_t index, uint64_t flags)
{
    page_table_entry_t entry = parent->pages[index];

    if ((entry & PAGE_TABLE_ENTRY_PRESENT) != 0)
    {
        if ((entry & PAGE_TABLE_ENTRY_PAGE_SIZE) != 0)
        {
            return NULL;
        }

        return page_table_from_entry(entry);
    }

    physaddr_t physical;
    page_table_t *child = page_table_create(&physical);

    if (child == NULL)
        return NULL;

    uint64_t table_flags = PAGE_TABLE_ENTRY_PRESENT | PAGE_TABLE_ENTRY_WRITE;

    if ((flags & PAGE_TABLE_ENTRY_USER) != 0)
        table_flags |= PAGE_TABLE_ENTRY_USER;

    parent->pages[index] = physical | table_flags;

    return child;
}

static int page_map(page_table_t *root, uintptr_t virtual_address, physaddr_t physical_address, unsigned int flags)
{
    if (root == NULL)
        return -1;

    if ((virtual_address & (PAGE_SIZE - 1)) != 0)
        return -1;

    if ((physical_address & (PAGE_SIZE - 1)) != 0)
        return -1;

    const uint16_t p4_index = PAGE_ENTRY_INDEX(virtual_address, P4_INDEX_SHIFT);
    const uint16_t p3_index = PAGE_ENTRY_INDEX(virtual_address, P3_INDEX_SHIFT);
    const uint16_t p2_index = PAGE_ENTRY_INDEX(virtual_address, P2_INDEX_SHIFT);
    const uint16_t p1_index = PAGE_ENTRY_INDEX(virtual_address, P1_INDEX_SHIFT);

    page_table_t *page_table_l3 = page_table_next(root, p4_index, flags);

    if (page_table_l3 == NULL)
        return -1;

    page_table_t *page_table_l2 = page_table_next(page_table_l3, p3_index, flags);

    if (page_table_l2 == NULL)
        return -1;

    page_table_t *page_table_l1 = page_table_next(page_table_l2, p2_index, flags);

    if (page_table_l1 == NULL)
        return -1;

    if ((page_table_l1->pages[p1_index] & PAGE_TABLE_ENTRY_PRESENT) != 0)
        return -1;

    page_table_l1->pages[p1_index] = (physical_address & PAGE_4K_ADDRESS_MASK) | PAGE_TABLE_ENTRY_PRESENT | flags;

    __asm__ volatile("invlpg (%0)" ::"r"(virtual_address) : "memory");

    return 0;
}

static int page_unmap(page_table_t *root, uintptr_t virtual_address)
{
    if (root == NULL)
        return -1;

    if ((virtual_address & (PAGE_SIZE - 1)) != 0)
        return -1;

    const uint16_t p4_index = PAGE_ENTRY_INDEX(virtual_address, P4_INDEX_SHIFT);
    const uint16_t p3_index = PAGE_ENTRY_INDEX(virtual_address, P3_INDEX_SHIFT);
    const uint16_t p2_index = PAGE_ENTRY_INDEX(virtual_address, P2_INDEX_SHIFT);
    const uint16_t p1_index = PAGE_ENTRY_INDEX(virtual_address, P1_INDEX_SHIFT);

    page_table_entry_t entry4 = root->pages[p4_index];
    if ((entry4 & PAGE_TABLE_ENTRY_PRESENT) == 0 ||
        (entry4 & PAGE_TABLE_ENTRY_PAGE_SIZE) != 0)
        return -1;

    page_table_t *page_table_l3 = page_table_from_entry(entry4);
    page_table_entry_t entry3 = page_table_l3->pages[p3_index];
    if ((entry3 & PAGE_TABLE_ENTRY_PRESENT) == 0 ||
        (entry3 & PAGE_TABLE_ENTRY_PAGE_SIZE) != 0)
        return -1;

    page_table_t *page_table_l2 = page_table_from_entry(entry3);
    page_table_entry_t entry2 = page_table_l2->pages[p2_index];
    if ((entry2 & PAGE_TABLE_ENTRY_PRESENT) == 0 ||
        (entry2 & PAGE_TABLE_ENTRY_PAGE_SIZE) != 0)
        return -1;

    page_table_t *page_table_l1 = page_table_from_entry(entry2);
    if ((page_table_l1->pages[p1_index] & PAGE_TABLE_ENTRY_PRESENT) == 0)
        return -1;

    page_table_l1->pages[p1_index] = 0;

    __asm__ __volatile__("invlpg (%0)" ::"r"(virtual_address) : "memory");

    return 0;
}

static int page_translate(page_table_t *root, uintptr_t virtual_address, physaddr_t *physical_address)
{
    if (root == NULL || physical_address == NULL)
        return -1;

    const uint16_t p4_index = PAGE_ENTRY_INDEX(virtual_address, P4_INDEX_SHIFT);
    const uint16_t p3_index = PAGE_ENTRY_INDEX(virtual_address, P3_INDEX_SHIFT);
    const uint16_t p2_index = PAGE_ENTRY_INDEX(virtual_address, P2_INDEX_SHIFT);
    const uint16_t p1_index = PAGE_ENTRY_INDEX(virtual_address, P1_INDEX_SHIFT);

    page_table_entry_t entry4 = root->pages[p4_index];
    if ((entry4 & PAGE_TABLE_ENTRY_PRESENT) == 0 ||
        (entry4 & PAGE_TABLE_ENTRY_PAGE_SIZE) != 0)
        return -1;

    page_table_t *page_table_l3 = page_table_from_entry(entry4);
    page_table_entry_t entry3 = page_table_l3->pages[p3_index];
    if ((entry3 & PAGE_TABLE_ENTRY_PRESENT) == 0 ||
        (entry3 & PAGE_TABLE_ENTRY_PAGE_SIZE) != 0)
        return -1;

    page_table_t *page_table_l2 = page_table_from_entry(entry3);
    page_table_entry_t entry2 = page_table_l2->pages[p2_index];
    if ((entry2 & PAGE_TABLE_ENTRY_PRESENT) == 0)
        return -1;

    // Check if the entry2 indicates a large page (2MB) and handle it accordingly.
    if ((entry2 & PAGE_TABLE_ENTRY_PAGE_SIZE) != 0)
    {
        physaddr_t page_base = entry2 & PAGE_2M_ADDRESS_MASK;
        uintptr_t page_offset = virtual_address & (PAGE_2M_SIZE - 1);

        *physical_address = page_base + page_offset;

        return 0;
    }

    page_table_t *page_table_l1 = page_table_from_entry(entry2);
    page_table_entry_t entry1 = page_table_l1->pages[p1_index];
    if ((entry1 & PAGE_TABLE_ENTRY_PRESENT) == 0)
        return -1;

    physaddr_t page_base = entry1 & PAGE_4K_ADDRESS_MASK;
    uintptr_t page_offset = virtual_address & (PAGE_4K_SIZE - 1);

    *physical_address = page_base + page_offset;

    return 0;
}

static void page_fault_handler(isr_context_t *regs)
{
    uintptr_t virtual_address;
    fault_address_get(virtual_address);
    uint64_t error_code = regs->info & 0xFFFFFFFF;

    if (isr_from_user(regs))
    {
        kprintf("user page fault: %s %s at %p (thread %u, rip %p)\n",
                (error_code & 0x10) ? "exec" : (error_code & 0x2) ? "write"
                                                                  : "read",
                (error_code & 0x1) ? "protection violation" : "not present",
                (void *)virtual_address, scheduler_current()->id, (void *)regs->rip);
        process_exit_signal(SIGSEGV);
    }

    panic("Page fault at address: %p, error: %p", virtual_address, error_code);
}

/* ---- Architecture contract (include/arch/mmu.h) ---- */

static mmu_root_t kernel_root;

static uint64_t mmu_flags_to_pte(unsigned int flags)
{
    uint64_t pte = 0;

    if (flags & MMU_WRITE)
        pte |= PAGE_TABLE_ENTRY_WRITE;
    if (flags & MMU_USER)
        pte |= PAGE_TABLE_ENTRY_USER;
    if (flags & MMU_NOCACHE)
        pte |= PAGE_TABLE_ENTRY_CACHE_DISABLE;

    // TODO: Honor MMU_EXEC with the NX bit once EFER.NXE is enabled in boot;
    // until then every mapping is executable.

    return pte;
}

mmu_root_t *arch_mmu_kernel_root(void)
{
    return &kernel_root;
}

int arch_mmu_map(mmu_root_t *root, uintptr_t virtual_address, physaddr_t physical_address, unsigned int flags)
{
    if (root == NULL)
        return -1;

    return page_map(root->top, virtual_address, physical_address, mmu_flags_to_pte(flags));
}

int arch_mmu_unmap(mmu_root_t *root, uintptr_t virtual_address)
{
    if (root == NULL)
        return -1;

    return page_unmap(root->top, virtual_address);
}

int arch_mmu_translate(mmu_root_t *root, uintptr_t virtual_address, physaddr_t *physical_address)
{
    if (root == NULL)
        return -1;

    return page_translate(root->top, virtual_address, physical_address);
}

static inline physaddr_t cr3_read(void)
{
    physaddr_t value;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(value));
    return value & PAGE_4K_ADDRESS_MASK;
}

static inline void cr3_write(physaddr_t value)
{
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(value) : "memory");
}

/**
 * Free a user-half page table and everything below it.
 *
 * @param physical Physical address of the table.
 * @param level 3 for an L3 table, down to 1 for an L1 table (whose entries are pages).
 */
static void page_table_free(physaddr_t physical, int level)
{
    page_table_t *table = phys_to_kern(physical);

    for (unsigned int i = 0; i < PAGE_TABLE_ENTRIES; i++)
    {
        page_table_entry_t entry = table->pages[i];

        if ((entry & PAGE_TABLE_ENTRY_PRESENT) == 0)
            continue;

        if (level > 1 && (entry & PAGE_TABLE_ENTRY_PAGE_SIZE) != 0)
            panic("page_table_free: Huge page in a user address space\n");

        if (level > 1)
            page_table_free(entry & PAGE_4K_ADDRESS_MASK, level - 1);
        else
            frame_free(entry & PAGE_4K_ADDRESS_MASK, 0);
    }

    frame_free(physical, 0);
}

void arch_mmu_init(void)
{
    // Address spaces copy the kernel's L4 entries when created. An L4 entry
    // added later would be missing from all of them, so give every
    // kernel-half entry its L3 table now (256 pages, 1 MiB).
    for (unsigned int i = PAGE_TABLE_KERNEL_FIRST; i < PAGE_TABLE_ENTRIES; i++)
    {
        if ((page_table_l4.pages[i] & PAGE_TABLE_ENTRY_PRESENT) != 0)
            continue;

        physaddr_t physical;

        if (page_table_create(&physical) == NULL)
            panic("arch_mmu_init: Out of memory for kernel page tables\n");

        page_table_l4.pages[i] = physical | PAGE_TABLE_ENTRY_PRESENT | PAGE_TABLE_ENTRY_WRITE;
    }
}

mmu_root_t *arch_mmu_root_create(void)
{
    mmu_root_t *root = kzalloc(sizeof(*root));

    if (root == NULL)
        return NULL;

    physaddr_t physical;
    page_table_t *table = page_table_create(&physical);

    if (table == NULL)
    {
        kfree(root);
        return NULL;
    }

    // User half stays empty (page_table_create zeroes it); kernel half is shared.
    // No USER bit on these entries, so ring 3 can never reach the kernel half.
    for (unsigned int i = PAGE_TABLE_KERNEL_FIRST; i < PAGE_TABLE_ENTRIES; i++)
        table->pages[i] = page_table_l4.pages[i];

    root->top = table;
    root->top_physical = physical;

    return root;
}

void arch_mmu_root_destroy(mmu_root_t *root)
{
    if (root == NULL || root == &kernel_root)
        return;

    if (cr3_read() == root->top_physical)
        panic("arch_mmu_root_destroy: Attempt to destroy the active page table\n");

    for (unsigned int i = 0; i < PAGE_TABLE_KERNEL_FIRST; i++)
    {
        page_table_entry_t entry = root->top->pages[i];

        if ((entry & PAGE_TABLE_ENTRY_PRESENT) != 0)
            page_table_free(entry & PAGE_4K_ADDRESS_MASK, 3);
    }

    frame_free(root->top_physical, 0);
    kfree(root);
}

void arch_mmu_activate(mmu_root_t *root)
{
    if (root == NULL)
        root = &kernel_root;

    // Reloading CR3 flushes the TLB; skip it when nothing changes
    if (cr3_read() != root->top_physical)
        cr3_write(root->top_physical);
}

void paging_init()
{
    kernel_root.top = &page_table_l4;
    kernel_root.top_physical = (physaddr_t)&page_table_l4 - KERNEL_VIRTUAL_ADDRESS;

    isr_info_t page_fault_info = {
        .type = ISR_EXCEPTION,
        .handler = page_fault_handler,
    };

    isr_set_info(VECTOR_EXCEPTION_PAGE_FAULT, &page_fault_info);
}