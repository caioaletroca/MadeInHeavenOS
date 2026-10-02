# Session Handoff — MadeInHeavenOS

## Current state

Working x86-64 PC kernel with:

- Docker generic → custom MiHOS cross-toolchain pipeline (build only runs inside the Docker image)
- GRUB/Multiboot2 boot, higher-half kernel
- Physical memory map, zones, buddy allocator, early boot allocator
- `kmalloc`/`kfree` on slab size classes (16–1024 B) with buddy pages above that
- 4 KiB page map/unmap/translate behind an arch-neutral MMU contract
- PIC/PIT timer at 100 Hz, generic IRQ registration layer
- **Preemptive** round-robin scheduler (timer-driven), voluntary yield via software interrupt
- Kernel split into generic code, arch contracts and platform contracts, enforced by the build
- MM and scheduler self-tests passing in Bochs

Latest commits:

```text
b919366 test(sched): drop scheduler self-test start banner
d5cd3a9 feat(mm): replace bump kmalloc with slab-backed allocator
094b1d1 docs(architecture): update session handoff
da6b47d build(make): move arch compiler flags to per-arch config
b7c841b refactor(mm): add arch-neutral MMU contract
e0f58e0 refactor(boot): parse Multiboot2 into generic boot_info
39ba3e6 Mini rework for multi arch build
6bccc91 feat(sched): add timer-driven preemptive scheduling
```

## Architecture

### Layers

```text
kernel/
├── include/                      # GENERIC: no asm, no x86, no PC
│   ├── arch/                     # contract every arch implements
│   │   ├── arch.h                #   arch_init()
│   │   ├── context.h             #   arch_thread_context_init(), arch_yield()
│   │   ├── irq.h                 #   irq_register(irq, handler)
│   │   └── mmu.h                 #   arch_mmu_kernel_root/map/unmap/translate, MMU_* flags
│   ├── platform/platform.h       # contract every board implements
│   ├── boot_info.h               # protocol-independent boot data
│   └── sched/ mm/ driver/ ...
├── arch/x86/
│   ├── include/asm/              # PUBLIC arch interface: irq_flags.h cpu.h memory.h
│   ├── common/include/x86/       # PRIVATE: cpu gdt idt io isr paging vectors exceptions
│   └── x86_64/                   # boot, ISR stubs, arch.c, context.c, isr.c, paging.c, ...
├── platform/pc/                  # pic, pit, ps2, keyboard, vga, multiboot2, platform.c
├── sched/ mm/ driver/ sys/ selftest/
└── kmain.c
```

### Include rules (enforced in `kernel/Makefile`)

| Code in | Can include |
|---|---|
| every directory | `include/` and `<asm/...>` |
| `arch/` and `platform/` only | additionally `<x86/...>` |

A generic file including `<x86/...>` fails to compile. Inline asm must not appear outside `arch/`.
Platform-private headers (`pic.h`, `vga.h`, `multiboot2.h`) are quote-included from the same directory.

### Arch contract (what a new arch must provide)

| Piece | x86_64 implementation |
|---|---|
| Entry → `kmain(uintptr_t boot_handoff)` | `boot.S`, `boot64.S` (passes Multiboot2 address in `%rdi`) |
| Linker script defining `_kernel_physical_end` | `x86_64/linker.lds` |
| `<asm/memory.h>`: `PAGE_SIZE`, `KERNEL_VIRTUAL_ADDRESS`, `KERNEL_DIRECT_MAP_SIZE`, `KERNEL_SELFTEST_VIRTUAL_BASE` | `arch/x86/include/asm/memory.h` |
| `<asm/irq_flags.h>`: `irq_save/restore/enable/disable`, `irq_flags_t` | inline `pushfq/cli/sti` |
| `<asm/cpu.h>`: `cpu_idle/cpu_halt/cpu_breakpoint` | inline `hlt`, Bochs magic breakpoint |
| `arch_init()` | `arch.c`: IDT, exceptions, page fault handler, yield vector |
| `arch_thread_context_init()`, `arch_yield()` | `context.c` + `thread_entry.S` trampoline |
| `irq_register()` + dispatch to `scheduler_on_interrupt()` | `isr.c` |
| `arch_mmu_*` | `paging.c` (`struct mmu_root { top, top_physical }`) |
| Toolchain flags | `config/<ARCH>.mk` (`ARCH_CFLAGS`) |

### Platform contract

`platform_boot_info_init(handoff, info)`, `platform_init()`, `platform_timer_init(hz)`,
`platform_irq_enable(irq)`, `platform_irq_eoi(irq)`, plus a `screen` backend.

### Boot sequence (`kmain`)

```text
platform_boot_info_init → arch_init → platform_init (PIC, PS/2)
→ mm_init(&boot_info) [mmap_init + kmalloc_init] → scheduler_init
→ timer_init(TIMER_FREQUENCY_HZ) → irq_enable → mm_selftest → scheduler_selftest
```

## Memory management

```text
boot_info regions ──▶ mmap_init ──▶ zones (one buddy system each) ──▶ frame_alloc/free (physaddr_t)
                        │                                                  │
                        └─ boot_alloc: zone_t, frame_t[], bitmaps          ├─▶ slab caches ─▶ kmalloc ≤ 1024 B
                           (sealed by kmalloc_init)                        └─▶ kmalloc > 1024 B (2^order pages)
```

- **Physical layout:** `[kernel image][boot metadata][usable memory …]`. The boot metadata reservation
  is computed from the memory map (`mmap_metadata_size`), not a fixed size. Only memory inside
  `KERNEL_DIRECT_MAP_SIZE` (1 GiB) is handed to zones.
- **Bootstrap:** zones start fully allocated (`refs = 1`); `mmap_release_regions` frees every frame
  to build the free lists.
- **Boot allocator** (`mm/boot_alloc.c`): zeroed, permanent bump allocations for allocator metadata.
  `boot_alloc_seal()` (called by `kmalloc_init`) makes later use panic.
- **Frame API:** `frame_alloc/frame_free` take and return `physaddr_t` (0 = failure; frame 0 is never
  in a zone). `frame_lookup(phys)` returns the `frame_t`, which records `order` (head of a page
  allocation) and `slab` (owning slab page).
- **Slab:** one-page slabs with the `slab_t` header at offset 0 of its own page; objects start at
  `cache->offset`. Owner lookup is O(1) through `frame_lookup(...)->slab`. Double frees are detected.
- **kmalloc:** classes 16, 32, …, 1024 B (16-byte aligned); larger sizes use whole buddy blocks
  (page aligned). `kfree` tries `slab_free` first, then frees the page block by its recorded order.
  Slab objects are never page aligned, page allocations always are.
- Address helpers: `phys_to_kern()` / `kern_to_phys()` (direct map at `KERNEL_VIRTUAL_ADDRESS`).

## Scheduler

- Every saved thread context is an x86 interrupt frame (`isr_context_t`, 21 qwords incl. `SS:RSP`).
  `thread_t` only stores an opaque `void *context`.
- `isr_common` pushes all GPRs, calls `isr_handler`, and resumes whatever context it returns.
- Preemption: PIT IRQ → `timer_tick()` → `scheduler_tick()` sets reschedule;
  `isr_handler` calls `scheduler_on_interrupt()` on exit (never from CPU exceptions).
- Yield: `arch_yield()` raises `int $0x81` (interrupt gate, DPL 0), so yielded and preempted
  threads share one frame format. `context_switch.S` was removed.
- New threads start from a synthetic frame: `iretq` → `thread_entry` → `call thread_start`
  (keeps SysV 16-byte alignment).
- Ready queue guarded with `irq_save/irq_restore`. IRQ gates 32–47 are interrupt gates (IF cleared).
- EOI is sent before switching threads.

## Interrupt vectors (`x86/vectors.h`)

| Range | Use |
|---|---|
| 0x00–0x1F | CPU exceptions (page fault = 14) |
| 0x20–0x2F | PIC IRQs (timer = IRQ 0, keyboard = IRQ 1) |
| 0x80 | Reserved for syscalls |
| 0x81 | Scheduler yield |

## Major bugs fixed (history)

- Boot stack started at `stack_bottom` and overwrote `page_table_l2[511]` → start at `stack_top`.
- `buddy_alloc()` did not remove the block from the free list → duplicate frames.
- Preemption draft: missing `SS:RSP` in the iret frame, reversed `isr_context_t` order,
  mixed cooperative/interrupt frame formats → fixed by unifying on interrupt frames.
- `mmap_free` looped to `length - 1`, so the last available region never reached the buddy
  allocator; sort/merge underflowed on empty maps.
- Old bump `kmalloc` returned physical addresses as pointers (worked only through a leftover
  identity mapping); memory above the 1 GiB direct map was handed to zones.

## Recommended next steps

### Step 2 — Memory management debt (done)

Frame API on `physaddr_t`, `mmap_release_regions` fix, boot allocator replacing `KERNEL_HEAP_SIZE`,
per-frame ownership, on-slab headers, `kmalloc`/`kfree`.

### Step 3 — Threads and synchronization (next)

- `thread_create(entry, arg)` with allocated stacks, unique IDs, reaper for terminated threads.
- Wait queues + `THREAD_BLOCKED`, `thread_sleep(ticks)`, mutex, semaphore.
- Keyboard IRQ → ring buffer + wake reader (no `kprintf` in IRQ context).
- Consider the two-level (Linux/xv6) context switch once blocking makes yield frequent.

### Step 4 — User mode

`address_space_t` (builds on `mmu_root`), ring-3 GDT segments + TSS `rsp0` update on switch,
`int 0x80` syscalls (`IDT_GATE_USER_INTERRUPT`), first user program, user faults kill the process.

### Optional — second arch skeleton

A stub `riscv64`/`qemu-virt` (or custom CPU) target implementing the contracts with stubs and a UART
`screen` backend. Link errors reveal remaining x86 assumptions.

## Known technical debt

- `page_unmap` does not reclaim empty intermediate tables.
- Direct map covers only the first 1 GiB; RAM above it is ignored (clamped in `mmap_init`).
- `MMU_EXEC` ignored: NX not enabled (`EFER.NXE`), every mapping is executable.
- Slab pages are never returned to the buddy allocator (no empty-slab reclamation, no cache destroy).
- `kmalloc` above 1024 B rounds up to a power-of-two number of pages (internal waste).
- `kmalloc`/slab have no locking; only safe because callers do not allocate from IRQ context yet.
- `frame_free`/`frame_lookup` walk the zone list linearly.
- Terminated thread stacks are not reclaimed; `thread->id` is always 0.
- No SMP; `irq_save` is the only synchronization.
- `driver/ps2.c` / `driver/ps2.h` are PC-only code in the generic `driver/` dir;
  `ps2.h` defines a `static` array in a header.
- `<asm/...>` lives at family level (`arch/x86/include`) but uses x86_64-only instructions.
- Kernel `install-headers` copies internal headers into the sysroot (stale copies can hide
  include errors → use a clean build after moving headers).
- `ARCH_CFLAGS` (`-mcmodel=kernel`) also applies to the future userland libc.

## Commands

Build (inside the Docker image):

```sh
make clean
make all
make print-config
```

Run Bochs:

```sh
bochs -q -f .bochsrc
```

Local, intentionally uncommitted: `.bochsrc`, `bx_enh_dbg.ini`.
