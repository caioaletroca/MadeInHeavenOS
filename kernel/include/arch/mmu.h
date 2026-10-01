#ifndef _ARCH_MM_H_
#define _ARCH_MM_H_

#include <stdint.h>
#include <addresses.h>

/*
 * Architecture contract: virtual memory mappings.
 */

// Generic mapping flags; each arch translates them to its own entry bits.
#define MMU_WRITE (1u << 0)   // Writable (read-only otherwise)
#define MMU_USER (1u << 1)    // Accessible from user mode
#define MMU_EXEC (1u << 2)    // Executable
#define MMU_NOCACHE (1u << 3) // Uncached (device memory)

/**
 * @brief Root of a translation hierarchy (opaque, arch-defined).
 */
typedef struct mmu_root mmu_root_t;

/**
 * @brief Get the kernel's root of the translation hierarchy.
 *
 * @return Pointer to the kernel's mmu_root_t.
 */
mmu_root_t *arch_mmu_kernel_root(void);

/**
 * @brief Map a virtual address to a physical address in the given translation hierarchy.
 *
 * @param root Pointer to the root of the translation hierarchy.
 * @param virtual_address Virtual address to map.
 * @param physical_address Physical address to map to.
 * @param flags Mapping flags (MMU_WRITE, MMU_USER, MMU_EXEC, MMU_NOCACHE).
 * @return 0 on success, negative value on failure.
 */
int arch_mmu_map(mmu_root_t *root, uintptr_t virtual_address, uintptr_t physical_address, unsigned int flags);

/**
 * @brief Unmap a virtual address in the given translation hierarchy.
 *
 * @param root Pointer to the root of the translation hierarchy.
 * @param virtual_address Virtual address to unmap.
 * @return 0 on success, negative value on failure.
 */
int arch_mmu_unmap(mmu_root_t *root, uintptr_t virtual_address);

/**
 * @brief Translate a virtual address to a physical address in the given translation hierarchy.
 *
 * @param root Pointer to the root of the translation hierarchy.
 * @param virtual_address Virtual address to translate.
 * @param physical_address Pointer to store the resulting physical address.
 * @return 0 on success, negative value on failure.
 */
int arch_mmu_translate(mmu_root_t *root, uintptr_t virtual_address, uintptr_t *physical_address);

#endif // _ARCH_MM_H_