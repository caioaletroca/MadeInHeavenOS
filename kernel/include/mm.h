#ifndef _MM_H_
#define _MM_H_

#include <stdint.h>
#include <multiboot2.h>
#include <util.h>

void mmap_init(struct multiboot_info *info);

#endif