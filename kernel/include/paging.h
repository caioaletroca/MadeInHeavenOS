#ifndef _PAGING_H_
#define _PAGING_H_

#include <memory.h>
#include <addresses.h>
#include <isr.h>

/*
 * Memory page size
 */
#define PAGE_SIZE 0x1000

/**
 * Page Table/Directory entry flags
 */

// Bit representing if the page is present
#define PAGE_TABLE_ENTRY_PRESENT (1ULL << 0)

// Bit representing if the page is write (true)
// or read-only (false)
#define PAGE_TABLE_ENTRY_WRITE (1ULL << 1)

// Bit representing if the page is accessible by user (true)
// or supervisor/kernel (false)
#define PAGE_TABLE_ENTRY_USER (1ULL << 2)

// Bit representing the page size, 4 MB (true) or 4 KB (false)
#define PAGE_TABLE_ENTRY_PAGE_SIZE (1ULL << 7)

typedef uint64_t page_table_entry_t;

/**
 * Represents a page table.
 */
typedef struct page_table
{
    page_table_entry_t pages[512];
} __attribute__((__packed__, __aligned__(PAGE_SIZE))) page_table_t;

/**
 * Maps a virtual address to a physical address in the given page table root.
 *
 * @param root The root page table (typically the P4 table).
 * @param virtual_address The virtual address to map.
 * @param physical_address The physical address to map to.
 * @param flags The page table entry flags.
 * @return 0 on success, non-zero on failure.
 */
int page_map(page_table_t *root, uintptr_t virtual_address, physaddr_t physical_address, unsigned int flags);

/**
 * Unmaps a virtual address from the given page table root.
 *
 * @param root The root page table (typically the P4 table).
 * @param virtual_address The virtual address to unmap.
 * @return 0 on success, non-zero on failure.
 */
int page_unmap(page_table_t *root, uintptr_t virtual_address);

void paging_init();

#endif