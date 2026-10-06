#include <mm.h>
#include <mm/mmap.h>
#include <util.h>
#include <kprintf.h>

#define MMAP_MAX_REGIONS 128

extern const char _kernel_physical_end;

mmap_region_t available[MMAP_MAX_REGIONS];
mmap_region_t reserved[MMAP_MAX_REGIONS];

static mmap_t memory_map = {
    .available = {.length = 0, .regions = available},
    .reserved = {.length = 0, .regions = reserved}};

/**
 * Checks if the given memory range is available according to the boot information.
 *
 * @param info The boot information containing memory regions.
 * @param start The start address of the memory range.
 * @param end The end address of the memory range.
 * @return true if the memory range is available, false otherwise.
 */
static bool mmap_is_available(const boot_info_t *info, physaddr_t start, physaddr_t end)
{
    for (size_t i = 0; i < info->memory_region_count; i++)
    {
        const boot_memory_region_t *region = &info->memory_regions[i];

        if (region->type == BOOT_MEMORY_AVAILABLE && start >= region->base && end <= region->base + region->length)
            return true;
    }
    return false;
}

/**
 * Releases every frame of the given regions to the frame allocator.
 * Zones start fully allocated; this is what populates their free lists.
 */
static void mmap_release_regions(const mmap_type_t *type)
{

    for (size_t i = 0; i < type->length; i++)
    {
        uintptr_t start = type->regions[i].base;
        uintptr_t end = start + type->regions[i].size - 1;

        for (physaddr_t current = start; current < end; current += PAGE_SIZE)
        {
            frame_free(current, 0);
        }
    }
}

static void mmap_swap_region(mmap_region_t *x, mmap_region_t *y)
{
    mmap_region_t temp = *x;
    *x = *y;
    *y = temp;
}

static void mmap_sort_region(mmap_type_t *type)
{
    if (type->length < 2)
        return;

    bool swapped = false;

    // Loops through all regions
    for (size_t i = 0; i < type->length - 1; i++)
    {
        swapped = false;

        // Loops again
        for (size_t j = 0; j < type->length - i - 1; j++)
        {
            // Check if the current base address is higher than the next one
            // Swap regions if needed
            if (type->regions[j].base > type->regions[j + 1].base)
            {
                mmap_swap_region(&type->regions[j], &type->regions[j + 1]);
                swapped = true;
            }
        }

        // Breaks loop earlier if no swaps happened
        if (swapped == false)
        {
            break;
        }
    }
}

static void mmap_merge_region(mmap_type_t *type)
{
    if (type->length < 2)
        return;

    size_t i = type->length - 1;

    // Loops in descending order through all regions
    while (i > 0)
    {
        uintptr_t current_address = type->regions[i].base;
        size_t current_size = type->regions[i].size;

        // If the region before end address is equal to the current address
        // That means we can merge those two regions
        if (type->regions[i - 1].base + type->regions[i - 1].size == current_address)
        {
            // Merge the sizes
            type->regions[i - 1].size += current_size;

            // Remove the current and now extra region
            mmap_remove_region(type, i);
        }

        i--;
    }
}

static void mmap_split_region(mmap_type_t *type, size_t frame_size)
{
    // Loop through all regions
    for (size_t i = 0; i < type->length; i++)
    {
        uintptr_t current_address = type->regions[i].base;
        size_t current_size = type->regions[i].size;

        // Calculate the max order of this size
        size_t frame_num = current_size / frame_size;
        unsigned int order_max = _fnzb(frame_num);

        // The new size needs to be divisible by 2^(order_max)
        size_t new_size = ((size_t)1 << order_max) * frame_size;

        // Update the current region
        type->regions[i].size = new_size;

        // If this new split is around the 4th order,
        // That means the rest of space is smaller than 4 frame sizes,
        // so ignore this extra space, is now unmapped memory
        if (order_max >= 4)
        {
            // Insert the rest of the memory available into a new region
            // Offsetting the address by the new size, and using the rest of the size
            mmap_insert_region(type, current_address + new_size, current_size - new_size);
        }
    }
}

static void mmap_remove_region(mmap_type_t *type, size_t i)
{
    memmove(type->regions + i, type->regions + i + 1, (type->length - i - 1) * sizeof(mmap_region_t));
    type->length--;
}

static void mmap_insert_region(mmap_type_t *type, uintptr_t address, size_t size)
{
    if (size == 0)
    {
        return;
    }

    // Adds new region to the end of array
    const mmap_region_t new_region = {
        .base = address,
        .size = size};
    type->regions[type->length++] = new_region;
}

static void mmap_register_region(mmap_type_t *type, size_t frame_size)
{
    for (size_t i = 0; i < type->length; i++)
    {
        frame_zone_add(type->regions[i].base, type->regions[i].size, frame_size);
    }
}

/**
 * Calculate the size of the metadata required for the memory map.
 * This includes the size of frame structures, zone structures, and other bookkeeping data.
 *
 * @param info Pointer to the boot information structure containing memory regions.
 * @return The total size of the metadata required.
 */
static size_t mmap_metadata_size(const boot_info_t *info)
{
    size_t frames = 0;

    for (size_t i = 0; i < info->memory_region_count; i++)
    {
        const boot_memory_region_t *region = &info->memory_regions[i];

        if (region->type != BOOT_MEMORY_AVAILABLE || region->base >= KERNEL_DIRECT_MAP_SIZE)
            continue;

        physaddr_t end = region->base + region->length;
        if (end > KERNEL_DIRECT_MAP_SIZE)
            end = KERNEL_DIRECT_MAP_SIZE;

        frames += (end - region->base) / PAGE_SIZE;
    }

    const size_t orders = sizeof(unsigned long) * 8;
    const size_t per_zone = sizeof(zone_t) + orders * sizeof(free_list_t) + orders * sizeof(unsigned long) + 4 * 16;

    return frames * sizeof(frame_t) + frames / 8 + MMAP_MAX_REGIONS * per_zone + PAGE_SIZE;
}

void mmap_init(const boot_info_t *info)
{
    physaddr_t kernel_end = ALIGN_UP((uintptr_t)&_kernel_physical_end, PAGE_SIZE);
    size_t boot_size = ALIGN_UP(mmap_metadata_size(info), PAGE_SIZE);

    physaddr_t image_end = kernel_end;
    for (size_t i = 0; i < info->module_count; i++)
    {
        const boot_module_t *module = &info->modules[i];

        if (module->end > KERNEL_DIRECT_MAP_SIZE)
            panic("mmap: module '%s' exceeds the kernel direct map size", module->name);

        image_end = MAX(image_end, ALIGN_UP(module->end, PAGE_SIZE));
    }

    if (!mmap_is_available(info, image_end, image_end + boot_size))
        panic("mmap: no room for %u bytes of boot metadata after the kernel and modules", (unsigned int)boot_size);

    boot_alloc_init(image_end, boot_size);

    physaddr_t usable_start = image_end + boot_size;

    for (size_t i = 0; i < info->memory_region_count; i++)
    {
        const boot_memory_region_t *region = &info->memory_regions[i];
        if (region->type != BOOT_MEMORY_AVAILABLE)
            continue;

        physaddr_t start = region->base;
        physaddr_t end = region->base + region->length;

        if (end > KERNEL_DIRECT_MAP_SIZE)
            end = KERNEL_DIRECT_MAP_SIZE;

        if (end <= usable_start)
        {
            if (start < end)
                mmap_insert_region(&memory_map.reserved, start, end - start);

            continue;
        }

        if (start < usable_start)
            start = usable_start;

        start = ALIGN_UP(start, PAGE_SIZE);
        end = ALIGN_DOWN(end, PAGE_SIZE);

        if (start < end)
            mmap_insert_region(&memory_map.available, start, end - start);
    }

    mmap_sort_region(&memory_map.available);
    mmap_merge_region(&memory_map.available);
    mmap_split_region(&memory_map.available, PAGE_SIZE);
    mmap_sort_region(&memory_map.available);

    mmap_register_region(&memory_map.available, PAGE_SIZE);

    mmap_release_regions(&memory_map.available);
}

static void mmap_log(mmap_t *ctx)
{
    kprintf("- Memory Map -\n");

    kprintf("Reserved:    [ %u ]\n", ctx->reserved.length);
    for (size_t i = 0; i < ctx->reserved.length; i++)
    {
        mmap_region_t *region = &ctx->reserved.regions[i];
        kprintf("    [ 0x%p : 0x%p ] %u bytes\n", region->base, region->base + region->size - 1, region->size);
    }

    kprintf("Available:   [ %u ]\n", ctx->available.length);
    for (size_t i = 0; i < ctx->available.length; i++)
    {
        mmap_region_t *region = &ctx->available.regions[i];
        kprintf("    [ 0x%p : 0x%p ] %u bytes\n", region->base, region->base + region->size - 1, region->size);
    }
}