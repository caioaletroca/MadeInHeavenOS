#ifndef _PAGING_H_
#define _PAGING_H_

#include <memory.h>
#include <isr.h>

/*
 * Memory page size
 */
#define PAGE_SIZE       0x1000

/**
 * Page Table/Directory entry flags
*/

// Bit representing if the page is present
#define PAGE_TABLE_ENTRY_PRESENT                (1 << 0)

// Bit representing if the page is write (true)
// or read-only (false)
#define PAGE_TABLE_ENTRY_WRITE                  (1 << 1)

// Bit representing if the page is accessible by user (true)
// or supervisor/kernel (false)
#define PAGE_TABLE_ENTRY_USER                   (1 << 2)

// Bit representing the page size, 4 MB (true) or 4 KB (false)
#define PAGE_TABLE_ENTRY_PAGE_SIZE              (1 << 7)

typedef uint64_t page_table_entry_t;

typedef struct page_table {
    page_table_entry_t pages[512];
} __attribute__((__packed__, __aligned__(PAGE_SIZE))) page_table_t;

void paging_init();

#endif