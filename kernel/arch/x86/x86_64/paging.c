#include <asm/paging.h>
#include <mm/frame.h>
#include <panic.h>
#include <string.h>
#include <x86/isr.h>

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
    physaddr_t physical = (physaddr_t)frame_alloc(0, 0);

    if (physical == 0)
        return NULL;

    page_table_t *table = phys_to_kern(physical);
    if (table == NULL)
    {
        frame_free((void *)physical, 0);
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

int page_map(page_table_t *root, uintptr_t virtual_address, physaddr_t physical_address, unsigned int flags)
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

int page_unmap(page_table_t *root, uintptr_t virtual_address)
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

int page_translate(page_table_t *root, uintptr_t virtual_address, physaddr_t *physical_address)
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

void page_fault_handler(isr_context_t *regs)
{
    uintptr_t virtual_address;
    fault_address_get(virtual_address);

    panic("Page fault at address: %p, error: %p", virtual_address, regs->info & 0xFFFFFFFF);
}

void paging_init()
{
    isr_info_t page_fault_info = {
        .type = ISR_EXCEPTION,
        .handler = page_fault_handler,
    };

    isr_set_info(14, &page_fault_info);
}