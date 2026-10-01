#include <mm.h>
#include <mm/mmap.h>
#include <util.h>

#define MMAP_MAX_REGIONS 128

extern const uintptr_t _kernel_physical_end;

mmap_region_t available[MMAP_MAX_REGIONS];
mmap_region_t reserved[MMAP_MAX_REGIONS];

static mmap_t memory_map = {
    .available = {.length = 0, .regions = available},
    .reserved = {.length = 0, .regions = reserved}};

// TODO: See what to do with this function, maybe remove?
static void mmap_free(mmap_type_t *type)
{

    for (size_t i = 0; i < type->length - 1; i++)
    {
        uintptr_t start = type->regions[i].base;
        uintptr_t end = start + type->regions[i].size - 1;
        uintptr_t current = start;

        while (current < end)
        {
            frame_free(current, 0);
            current += PAGE_SIZE;
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
        size_t new_size = (1 << order_max) * frame_size;

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

void mmap_init(const boot_info_t *info)
{
    uintptr_t kern_end = (uintptr_t)&_kernel_physical_end + KERNEL_HEAP_SIZE;

    for (size_t i = 0; i < info->memory_region_count; i++)
    {
        const boot_memory_region_t *region = &info->memory_regions[i];
        if (region->type != BOOT_MEMORY_AVAILABLE)
            continue;

        // Region entirely below the kernel image + bootstrap heap
        if (region->base + region->length < kern_end)
        {
            mmap_insert_region(&memory_map.reserved, region->base, region->length);
        }
        // Now, if the zone overlaps with the kernel, relocate the base address
        // to be after the Kernel
        else if (region->base < kern_end)
        {
            uintptr_t base = ALIGN_UP(kern_end, PAGE_SIZE);
            uintptr_t end = ALIGN_DOWN(region->base + region->length, PAGE_SIZE);

            if (end > base)
                mmap_insert_region(&memory_map.available, base, end - base);
        }
        // The entry is available
        else
        {
            uintptr_t base = ALIGN_UP(region->base, PAGE_SIZE);
            size_t length = ALIGN_DOWN(region->length, PAGE_SIZE);

            mmap_insert_region(&memory_map.available, base, length);
        }
    }

    mmap_sort_region(&memory_map.available);
    mmap_merge_region(&memory_map.available);
    mmap_split_region(&memory_map.available, PAGE_SIZE);
    mmap_sort_region(&memory_map.available);

    mmap_register_region(&memory_map.available, PAGE_SIZE);

    mmap_free(&memory_map.available);
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