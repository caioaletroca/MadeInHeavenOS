#ifndef _GDT_H_
#define _GDT_H_

#define GDT_ENTRY_SIZE 8
#define GDT_RPL_KERNEL 0
#define GDT_RPL_USER 3

// Must match the descriptor order in gdt64.S
#define GDT_INDEX_NULL 0
#define GDT_INDEX_KCODE 1
#define GDT_INDEX_KDATA 2

#define GDT_SELECTOR(index, rpl) (((index) * GDT_ENTRY_SIZE) | (rpl))

#define KERNEL_CODE_SELECTOR GDT_SELECTOR(GDT_INDEX_KCODE, GDT_RPL_KERNEL) // 0x08
#define KERNEL_DATA_SELECTOR GDT_SELECTOR(GDT_INDEX_KDATA, GDT_RPL_KERNEL) // 0x10

#endif // _GDT_H_