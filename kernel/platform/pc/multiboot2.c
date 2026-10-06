#include <boot_info.h>
#include <addresses.h>
#include <panic.h>
#include <string.h>
#include <util.h>
#include <platform/platform.h>
#include "multiboot2.h"

/**
 * @brief Convert a Multiboot2 memory type to a boot_memory_type_t.
 *
 * @param type Multiboot2 memory type.
 * @return Corresponding boot_memory_type_t value.
 */
static boot_memory_type_t multiboot_memory_type(multiboot_uint32_t type)
{
    switch (type)
    {
    case MULTIBOOT_MEMORY_AVAILABLE:
        return BOOT_MEMORY_AVAILABLE;
    case MULTIBOOT_MEMORY_RESERVED:
        return BOOT_MEMORY_RESERVED;
    case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE:
        return BOOT_MEMORY_ACPI_RECLAIMABLE;
    case MULTIBOOT_MEMORY_NVS:
        return BOOT_MEMORY_ACPI_NVS;
    case MULTIBOOT_MEMORY_BADRAM:
        return BOOT_MEMORY_BAD;
    default:
        return BOOT_MEMORY_RESERVED;
    }
}

/**
 * @brief Parse the memory map from a Multiboot2 tag and populate the boot_info structure.
 *
 * @param tag Pointer to the Multiboot2 memory map tag.
 * @param info Pointer to the boot_info structure to populate.
 */
static void multiboot_parse_mmap(const struct multiboot_tag_mmap *tag, boot_info_t *info)
{
    const uint8_t *end = (const uint8_t *)tag + tag->size;

    for (const uint8_t *cursor = (const uint8_t *)tag->entries; cursor < end; cursor += tag->entry_size)
    {
        const struct multiboot_mmap_entry *entry = (const struct multiboot_mmap_entry *)cursor;

        if (info->memory_region_count == BOOT_MEMORY_MAX_REGIONS)
            panic("boot: more than %u memory regions", BOOT_MEMORY_MAX_REGIONS);

        boot_memory_region_t *region = &info->memory_regions[info->memory_region_count++];
        region->base = entry->base_address;
        region->length = entry->length;
        region->type = multiboot_memory_type(entry->type);
    }
}

/**
 * @brief Parse the command line from a Multiboot2 tag and populate the boot_info structure.
 *
 * @param tag Pointer to the Multiboot2 command line tag.
 * @param info Pointer to the boot_info structure to populate.
 */
static void multiboot_parse_cmdline(const struct multiboot_tag_string *tag, boot_info_t *info)
{
    strlcpy(info->cmdline, tag->string, sizeof(info->cmdline));
}

/**
 * @brief Parse a module from a Multiboot2 tag and populate the boot_info structure.
 *
 * @param tag Pointer to the Multiboot2 module tag.
 * @param info Pointer to the boot_info structure to populate.
 */
static void multiboot_parse_module(const struct multiboot_tag_module *tag, boot_info_t *info)
{
    if (info->module_count == BOOT_MODULE_MAX)
        panic("boot: more than %u modules", BOOT_MODULE_MAX);

    if (tag->mod_end < tag->mod_start)
        panic("boot: module end is before module start");

    boot_module_t *module = &info->modules[info->module_count++];
    module->start = tag->mod_start;
    module->end = tag->mod_end;

    strlcpy(module->name, tag->string, sizeof(module->name));
}

void platform_boot_info_init(uintptr_t handoff, boot_info_t *info)
{
    memset(info, 0, sizeof(*info));

    const struct multiboot_info *mbi = phys_to_kern((physaddr_t)handoff);
    if (mbi == NULL)
        panic("boot: no Multiboot2 information structure");

    for (const struct multiboot_tag *tag = mbi->tags;
         tag->type != MULTIBOOT_TAG_END;
         tag = (const struct multiboot_tag *)ALIGN_UP((uintptr_t)tag + tag->size, MULTIBOOT_TAG_ALIGN))
    {
        switch (tag->type)
        {
        case MULTIBOOT_TAG_MMAP:
            multiboot_parse_mmap((const struct multiboot_tag_mmap *)tag, info);
            break;
        case MULTIBOOT_TAG_CMDLINE:
            multiboot_parse_cmdline((const struct multiboot_tag_string *)tag, info);
            break;
        case MULTIBOOT_TAG_MODULE:
            multiboot_parse_module((const struct multiboot_tag_module *)tag, info);
            break;
        default:
            break;
        }
    }
}