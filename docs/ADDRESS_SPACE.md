# Address Space

Canonical offset: `KERNEL_VIRTUAL_ADDRESS 0xFFFFFFFF80000000` — `kernel/include/memory.h:4-8`.

Conversion: `phys + KERNEL_VIRTUAL_ADDRESS`, NULL stays NULL — `kernel/include/addresses.h:18-22`.

There are 3 copies of this transform:

- `kernel/include/addresses.h:18-22` `phys_to_kern`
- `kernel/mm/paging.c:20-25` private `phys_to_virt`
- `kernel/mm/slab.c:67-70` private `phys_to_virt`, no NULL check

## Linker

`kernel/arch/x86_64/linker.lds:1-43`

```text
physical = 1M
offset   = KERNEL_VIRTUAL_ADDRESS
virtual  = offset + physical
ENTRY(_start)
.rodata (.multiboot first) / .text / .data / .bss, ALIGN(4K), AT(ADDR-offset)
_kernel_physical_end = . + 0x1000 - offset
```

Preprocessed via `kernel/Makefile:38-39` to expand `memory.h`.

## Boot map

`kernel/arch/x86_64/boot.S:219-247`

- `L4[0]+L4[511] = L3`
- `L3[0]+L3[510] = L2`
- `L2[0..511] = 2MiB huge pages from phys 0`

Result: first 1 GiB identity-mapped + higher-half mapped. `boot64.S:55-84` switches to high `rsp`, tries to unmap low, flushes TLB, calls `kmain(phys)`.

`kmain.c:11-12` converts Multiboot ptr via `phys_to_kern`.

## Invariants to remember

- Only first 1 GiB has a direct map. `frame_alloc` can return phys above 1 GiB; slab blindly adds offset — `kernel/mm/slab.c:79-87`. No backing.
- `kmalloc.c:3-13` returns raw `_kernel_physical_end`-based pointer and callers dereference it (`mm/frame.c:5-18`, `mm/buddy.c:130-159`). If low map is gone, this is suspect. Verify before trusting.
- Heap reservation is fixed 64 KiB: `KERNEL_HEAP_SIZE 0x10000` — `kernel/include/memory.h:7-8`, used at `kernel/mm/mmap.c:139`.
- Page size: `PAGE_SIZE 0x1000` — `kernel/include/memory.h:4`.
