#ifndef _FRAME_H_
#define _FRAME_H_

#include <stddef.h>
#include <stdint.h>
#include <mm/zone.h>
#include <kmalloc.h>

int frame_zone_add(void *address, size_t size, size_t frame_size);

#endif