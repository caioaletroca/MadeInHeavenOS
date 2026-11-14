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

/**
 * @brief Get a mapped page ready for a write. A page shared by copy-on-write
 * becomes private and writable.
 *
 * @param root Pointer to the root of the translation hierarchy.
 * @param virtual_address Virtual address inside the page.
 * @return 0 if the page can be written, negative value if it is not mapped,
 * read-only for good, or there is no memory for the copy.
 */
int arch_mmu_prepare_write(mmu_root_t *root, uintptr_t virtual_address);

/**
 * @brief Change a mapped page's protection, keeping its frame and contents.
 * Without MMU_USER the page stays mapped, but user mode cannot reach it.
 * MMU_WRITE on a frame other spaces share makes it copy-on-write, not writable.
 *
 * @param root Pointer to the root of the translation hierarchy.
 * @param virtual_address Virtual address of the page (page aligned).
 * @param flags Mapping flags (MMU_WRITE, MMU_USER, MMU_EXEC).
 * @return 0 on success, negative value if the page is not mapped.
 */
int arch_mmu_protect(mmu_root_t *root, uintptr_t virtual_address, unsigned int flags);

/**
 * @brief Prepare the kernel half for sharing between address spaces.
 *
 * Must run after the frame allocator is up and before the first
 * arch_mmu_root_create().
 */
void arch_mmu_init(void);

/**
 * @brief Create a translation root with an empty user half and the shared kernel half.
 *
 * @return New root, or NULL if out of memory.
 */
mmu_root_t *arch_mmu_root_create(void);

/**
 * @brief Free a root created by arch_mmu_root_create(), including every page
 * mapped in its user half. Must not be the active root.
 *
 * @param root Root to destroy.
 */
void arch_mmu_root_destroy(mmu_root_t *root);

/**
 * @brief Copy the contents of one translation root to another.
 *
 * @param to Destination root.
 * @param from Source root.
 * @return 0 on success, negative value on failure.
 */
int arch_mmu_root_copy(mmu_root_t *to, mmu_root_t *from);

/**
 * @brief Switch the CPU to a translation root.
 *
 * @param root Root to activate, or NULL for the kernel root.
 */
void arch_mmu_activate(mmu_root_t *root);

#endif // _ARCH_MM_H_