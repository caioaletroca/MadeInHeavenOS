# Memory Management

Layers:

```text
Multiboot map -> mmap normalize -> zones -> buddy -> frame_alloc
  -> slab prototype
Paging: boot static map + vector-14 diagnostic handler
kmalloc: early bump allocator, not slab
```

Init order `kernel/kmain.c:22-26`: `mmap_init` -> `paging_init` -> `slab_init`.

## mmap

`kernel/mm/mmap.c:136-193`

- Walk Multiboot tags 8-byte aligned, only `MULTIBOOT_MEMORY_AVAILABLE`
- Reserve `[_kernel_physical_end, +64KiB)` via `KERNEL_HEAP_SIZE` — `mmap.c:139`
- Align, sort `mmap.c:38-61`, merge exact-adjacent `mmap.c:63-83`, split pow2 `mmap.c:85-110`, register zones `mmap.c:130-133`, free frames `mmap.c:18-29`
- Arrays fixed 128: `mmap.c:5-15`, no bounds check
- `mmap_free` loop `i < length-1` skips last region; underflows if length 0 — `mmap.c:20`

## Zone / Frame

`kernel/mm/frame.c:3-55`, `kernel/mm/zone.c`

- `frame_zone_add` uses `kmalloc`, ignores `zone_init` failure yet links zone — `frame.c:5-20`
- All zones `ZONE_NORMAL`, flags compared as bitmask but defined as categories — `include/mm/zone.h:9-37`, `frame.c:26-29`
- `frame_alloc(order,flags)` scans list; `frame_free` scans all, no stop after match
- Address: `zone base + index*frame_size` — `zone.c:12-13`

## Buddy

`kernel/mm/buddy.c:47-167`, contract `kernel/include/mm/buddy.h:18-64`

- Init all `refs=1`, lists empty, then `mmap_free` frees one-by-one to coalesce
- XOR buddy: `index ^ (1<<order)` — `buddy.c:99-123`
- Split on alloc: `buddy.c:80-87`
- No validation of pow2, order bounds, alignment, double-free; no locks
- Bitmap comment in Portuguese — `buddy.c:40-45`

Worked split: order-2 block -> alloc order-0 -> push right halves to order-1, order-0 lists, toggle bits.

## kmalloc

`kernel/kmalloc.c:3-13`: bump from `_kernel_physical_end`, align pointer-size, no free/bounds/NULL, never fails yet callers check NULL — `mm/frame.c:6-9`. See address-space concern in `ADDRESS_SPACE.md`.

## Slab — prototype, do not trust

`kernel/mm/slab.c`, contract `kernel/include/mm/slab.h:12-53`

- `slab_cache_cache` static avoids chicken-egg — `slab.c:8`
- `slab_cache_init` sizing — `slab.c:188-243`
- `gfp_order = _fnzb(size_bytes)` — `slab.c:238-242` — header says exponent-in-pages — `slab.h:38-40`. 4096B -> order 12 = 16MiB request. Wrong unit.
- `slab_space_alloc` — `slab.c:73-133`: uninit `i:110`, `bufctl->buf/slab` never set, small-object reserves pointer-size not struct-size, off-slab TODO, debug breakpoint `:101`
- `slab_cache_alloc` — `slab.c:135-186`: `list_container(&partial.next,...)` takes address-of-next, should take node; no NULL check after `bufctl_list_get`; no free path
- `bufctl_init` missing return — `slab.c:14-18`
- `BUF_TO_SLABCTL` assumes 4KiB unit — `slab.c:57-60`

## Paging — scaffold

`kernel/mm/paging.c:33-119`

- `paging_init` only `isr_set_info(14,...)`
- `page_map` prints PML4/L3/L2, ignores `phys/flags`, returns input, not in `include/paging.h:30-38`
- Fault handler reads CR2, calls `page_map`, unconditional `panic` — `paging.c:99-110`, no error-code decode
- Non-present entries still dereferenced for diagnostics — `paging.c:55-79`
