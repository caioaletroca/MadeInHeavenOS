#ifndef _X86_PAGING_H_
#define _X86_PAGING_H_

#include <stdint.h>
#include <addresses.h>
#include <arch/mmu.h>
#include <asm/memory.h>

#define PAGE_TABLE_ENTRIES 512

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

// Bit representing if the page is cache-disabled (true) or cache-enabled (false)
#define PAGE_TABLE_ENTRY_CACHE_DISABLE (1ULL << 4)

// Bit representing the page size, 2 MiB (true) or 4 KB (false)
#define PAGE_TABLE_ENTRY_PAGE_SIZE (1ULL << 7)

typedef uint64_t page_table_entry_t;

/**
 * Represents a page table.
 */
typedef struct page_table
{
    page_table_entry_t pages[PAGE_TABLE_ENTRIES];
} __attribute__((__packed__, __aligned__(PAGE_SIZE))) page_table_t;

struct mmu_root
{
    page_table_t *top;
    physaddr_t top_physical;
};

/**
 * @brief Set up the kernel root and install the page fault handler.
 */
void paging_init(void);

#endif // _X86_PAGING_H_