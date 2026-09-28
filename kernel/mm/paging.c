#include <paging.h>
#include <addresses.h>

extern page_table_t page_table_l4;
extern page_table_t page_table_l3;

#define fault_address_get(virt) \
    __asm__ __volatile__("mov %%cr2, %0" : "=r" (virt));

#define P1_INDEX_SHIFT 12
#define P2_INDEX_SHIFT 21
#define P3_INDEX_SHIFT 30
#define P4_INDEX_SHIFT 39

#define get_entry_index(address, shift) (((address) >> (shift)) & 0x1FFUL)

#define KERNEL_LOGICAL_BASE 0xFFFF800000000000
#define PTE_ADDR_MASK (~(0xFFF0000000000FFFUL))

static inline virtaddr_t phys_to_virt(physaddr_t p)
{
	if (p == (physaddr_t)NULL)
		return NULL;
	return (virtaddr_t)(p + KERNEL_VIRTUAL_ADDRESS);
}

static inline page_table_t *get_page_table(page_table_t *pg, uint16_t index) {
    page_table_entry_t entry = pg->pages[index];

    return phys_to_virt((entry & PTE_ADDR_MASK));
}

void *page_map(void *virt, void *phys, unsigned int flags) {
    const uintptr_t virt_page = (uintptr_t)virt & ~(PAGE_SIZE - 1);

    const uint16_t p4_index = get_entry_index(virt_page, P4_INDEX_SHIFT);
	const uint16_t p3_index = get_entry_index(virt_page, P3_INDEX_SHIFT);
	const uint16_t p2_index = get_entry_index(virt_page, P2_INDEX_SHIFT);
	const uint16_t p1_index = get_entry_index(virt_page, P1_INDEX_SHIFT);

    kprintf("Indexes: %u, %u, %u, %u\n", p4_index, p3_index, p2_index, p1_index);

    page_table_entry_t entry3 = page_table_l4.pages[p4_index];

    kprintf("Entry 3: 0x%p\n", entry3);

    // page_table_t *page_table_l3 = get_page_table(&page_table_l4, p4_index);
    // page_table_t *page_table_l2 = get_page_table(page_table_l3, p3_index);
    // page_table_t *page_table_l1 = get_page_table(page_table_l2, p2_index);

    // kprintf("P1 Table Address: 0x%p\n", page_table_l1);

    if((entry3 & 0b1) == 0) {
        kprintf("P3 NOT PRESENT\n");
    }

    page_table_t *page_table_l3_fake = phys_to_virt((entry3 & PTE_ADDR_MASK));
    kprintf("P3 Fake Table Address: 0x%p\n", page_table_l3_fake);
    kprintf("P3 Real Table Address: 0x%p\n", &page_table_l3);

    page_table_entry_t entry2 = page_table_l3_fake->pages[p3_index];

    kprintf("Entry 2: 0x%p\n", entry2);

    if((entry2 & 0b1) == 0) {
        kprintf("P2 NOT PRESENT\n");
    }

    page_table_t *page_table_l2 = phys_to_virt((entry2 & PTE_ADDR_MASK));
    kprintf("P2 Table Address: 0x%p\n", page_table_l2);

    page_table_entry_t entry1 = page_table_l2->pages[p2_index];
    kprintf("Entry 1: 0x%p\n", entry1);

    if((entry1 & 0b1) == 0) {
        kprintf("P1 NOT PRESENT\n");
    }

    // page_table_t *page_table_l1 = phys_to_virt((entry1 & PTE_ADDR_MASK));
    // kprintf("P1 Table Address: 0x%p\n", page_table_l1);

    // page_table_entry_t entry0 = page_table_l1->pages[p1_index];
    // kprintf("Entry 0: 0x%p\n", entry1);

    // if((entry0 & 0b1) == 0) {
    //     kprintf("P0 NOT PRESENT\n");
    // }

    // pte_t entry = pgtab->pages[index];
	// if ((entry & PAGE_PRESENT) == 0)
	// 	return NULL;
	// return phys_to_virt((entry & PTE_ADDR_MASK));

    return virt;
}

void page_fault_handler(isr_context_t *regs) {
    uintptr_t virt;
    fault_address_get(virt);

    kprintf("Address: %p\n", virt);

    // TODO: Check permissions

    page_map(virt, 0, 0);

    panic("PANIC ERROR: %p\n", (regs->info & 0xFFFFFFFF));
}

void paging_init() {
    isr_info_t page_fault_info = {
        .type = ISR_IRQ,
        .handler = page_fault_handler,
    };

    isr_set_info(14, &page_fault_info);
}