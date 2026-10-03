#ifndef _GDT_H_
#define _GDT_H_

#define GDT_ENTRY_SIZE 8
#define GDT_RPL_KERNEL 0
#define GDT_RPL_USER 3

// Must match the descriptor order in gdt64.S. User data comes before user
// code because sysret derives both from one base (SS = base + 8, CS = base + 16).
#define GDT_INDEX_NULL 0
#define GDT_INDEX_KCODE 1
#define GDT_INDEX_KDATA 2
#define GDT_INDEX_UDATA 3
#define GDT_INDEX_UCODE 4
#define GDT_INDEX_TSS 5 // 16-byte system descriptor: uses indices 5 and 6

#define GDT_SELECTOR(index, rpl) (((index) * GDT_ENTRY_SIZE) | (rpl))

#define KERNEL_CODE_SELECTOR GDT_SELECTOR(GDT_INDEX_KCODE, GDT_RPL_KERNEL) // 0x08
#define KERNEL_DATA_SELECTOR GDT_SELECTOR(GDT_INDEX_KDATA, GDT_RPL_KERNEL) // 0x10
#define USER_DATA_SELECTOR GDT_SELECTOR(GDT_INDEX_UDATA, GDT_RPL_USER)     // 0x1B
#define USER_CODE_SELECTOR GDT_SELECTOR(GDT_INDEX_UCODE, GDT_RPL_USER)     // 0x23
#define TSS_SELECTOR GDT_SELECTOR(GDT_INDEX_TSS, GDT_RPL_KERNEL)           // 0x28

#endif // _GDT_H_