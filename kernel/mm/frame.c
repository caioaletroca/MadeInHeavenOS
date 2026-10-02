#include <mm/frame.h>
#include <mm/boot_alloc.h>

static zone_t *zone_list;

int frame_zone_add(physaddr_t address, size_t size, size_t frame_size)
{
    zone_t *zone = boot_alloc(sizeof(zone_t), _Alignof(zone_t));
    if (zone == NULL)
    {
        return -1;
    }

    // TODO: Fix the flags param
    if (zone_init(zone, address, size, frame_size, ZONE_NORMAL) != 0)
    {
        // TODO: Free kmalloc memory here in case of failure
    }

    // Add newly created zone to the zone list
    zone->next = zone_list;
    zone_list = zone;

    return 0;
}

physaddr_t frame_alloc(unsigned int order, unsigned int flags)
{
    for (zone_t *zone = zone_list; zone != NULL; zone = zone->next)
    {
        if ((zone->flags & flags) != flags)
            continue;

        physaddr_t address = zone_alloc(zone, order);

        if (address != 0)
        {
            return address;
        }
    }

    return 0; // No suitable frame found
}

void frame_free(physaddr_t address, unsigned int order)
{
    if (address == 0)
        return;

    // Loop through all registered zones
    for (zone_t *zone = zone_list; zone != NULL; zone = zone->next)
    {
        // Sanity check for order
        if (order <= zone->buddy.order_max)
        {
            // Check if the address and order is in range of the current zone
            if (zone_contains(zone, address, (size_t)1 << (order + zone->buddy.order_bit)))
            {
                // Finally, free the memory
                zone_free(zone, address, order);
            }
        }
    }
}

frame_t *frame_lookup(physaddr_t address)
{
    for (zone_t *zone = zone_list; zone != NULL; zone = zone->next)
    {
        if (zone_contains(zone, address, 0))
            return &zone->buddy.frames[(address - zone->address) >> zone->buddy.order_bit];
    }

    return NULL; // Frame not found
}

void frame_log()
{
    for (zone_t *zone = zone_list; zone != NULL; zone = zone->next)
    {
        zone_log(zone);
    }
}