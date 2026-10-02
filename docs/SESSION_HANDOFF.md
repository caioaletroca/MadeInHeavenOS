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
- Dynamic kernel threads (`thread_create`), idle thread, reaper for exited threads
- Blocking primitives: wait queues, `thread_sleep`, semaphores, mutexes
- Generic input events; PS/2 scancode set 2 decoder feeding a blocking reader thread
- Kernel split into generic code, arch contracts and platform contracts, enforced by the build
- MM, preemption and sync self-tests passing in Bochs

Latest commits:

```text
63f16a2 feat(driver): add input events and PS/2 set 2 keyboard decoder
ef2923b feat(sched): add blocking primitives and thread lifecycle
9360111 fix(mm): make kmalloc and frame allocator preemption-safe
4bc55dd docs(architecture): document memory management in handoff
b919366 test(sched): drop scheduler self-test start banner
d5cd3a9 feat(mm): replace bump kmalloc with slab-backed allocator
da6b47d build(make): move arch compiler flags to per-arch config
b7c841b refactor(mm): add arch-neutral MMU contract
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
| `<asm/irq_flags.h>`: `irq_save/restore/enable/disable/enabled`, `irq_flags_t` | inline `pushfq/cli/sti` |
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
platform_boot_info_init → arch_init → input_init → platform_init (PIC, PS/2, keyboard IRQ)
→ mm_init(&boot_info) [mmap_init + kmalloc_init] → scheduler_init [idle thread]
→ threads_init [reaper] → thread_create(keyboard_reader)
→ timer_init(TIMER_FREQUENCY_HZ) → irq_enable → mm_selftest → scheduler_selftest [+ sync tests]
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
- **Locking:** `kmalloc/kfree/kzalloc` and `frame_alloc/frame_free` run under `irq_save`, so they are
  preemption-safe and callable from IRQ handlers (single CPU).

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

### Threads

- `thread_create(entry, arg)`: `kzalloc`'d `thread_t` + 16 KiB `kmalloc`'d stack, flagged
  `THREAD_FLAG_OWNED`. `thread_init(...)` sets up threads on caller-owned memory (idle, tests).
- IDs come from a global counter (boot thread = 0).
- `entry(arg)` returning → `thread_start` → `thread_exit()`.
- **Invariant:** `run_link` is in exactly one list — ready queue, a wait queue, the sleep queue or
  the zombie list — or none (the running thread, exited static threads).
- **Idle thread** (static stack, `cpu_idle` loop) runs only when nothing is ready; never queued.
- **Reaper:** `thread_exit` disables IRQs for good, marks the thread `TERMINATED`, and (if owned)
  puts it on `zombie_list` + `semaphore_up(zombie_count)`, then yields. The reaper thread sleeps on
  that semaphore and frees stack + struct. IF=0 guarantees the exiting thread is off its stack first.

### Blocking and synchronization (`sched/wait.c`, `sched/sync.c`)

- Single CPU: disabling interrupts is the global lock (no preemption, no handlers).
- `scheduler_block(list)`: current → `BLOCKED`, onto the list, `arch_yield()`. Must be called with
  IRQs off; returns after wake-up still with IRQs off (IF is saved per thread in its frame).
- `scheduler_wake(thread)`: `BLOCKED` → `READY` onto the ready queue; requests an immediate
  reschedule when the CPU is idling. Safe from IRQ handlers.
- Usage pattern (Mesa semantics, prevents lost wake-ups):
  ```c
  irq_flags_t flags = irq_save();
  while (!condition)
      wait_queue_sleep(&queue);
  irq_restore(flags);
  ```
- `wait_queue_t`: FIFO list of sleepers; `wake_one` / `wake_all` are IRQ-safe.
- `semaphore_t` = `count` + wait queue. `down` waits for and consumes one unit; `up` adds one unit
  and wakes one waiter (non-blocking, IRQ-safe, never loses signals).
- `mutex_t` = `owner` + wait queue; not recursive, only the owner may unlock.
- `thread_sleep(ticks)`: sleep queue checked on every `scheduler_tick` (own tick counter).
- **Never block in IRQ context** (`semaphore_down`, `mutex_lock`, `thread_sleep`): it would block the
  interrupted thread. Only `up`/`wake`/`input_report_key` are allowed there.

## Input

```text
IRQ1 → platform/pc/keyboard.c (set 2 decoder) → input_report_key(keycode, pressed)
     → driver/input.c ring buffer (128 events) + semaphore → input_get_event() (blocking, threads)
```

- `keycode_t` (`include/driver/input.h`) names physical key positions on a US layout, not characters
  (`KEY_SEMICOLON` types 'ç' on ABNT2). Covers 104 keys, ABNT2 extras (`KEY_102ND`, `KEY_RO`,
  `KEY_KP_COMMA`), keypad, media and ACPI keys.
- Decoder handles `E0` extended keys, `F0` releases, Print Screen fake shifts and the 8-byte Pause
  sequence (reported as press + release).
- Full buffer drops events. A `keyboard_reader` thread in `kmain` currently prints the events.

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
- `scheduler_wake` checked `current_thread->state` instead of the woken thread's, so wake-ups
  were silently dropped; thread entries were called without their argument.

## Recommended next steps

### Step 2 — Memory management debt (done)

Frame API on `physaddr_t`, `mmap_release_regions` fix, boot allocator replacing `KERNEL_HEAP_SIZE`,
per-frame ownership, on-slab headers, `kmalloc`/`kfree`.

### Step 3 — Threads and synchronization (done)

Allocator locking + `kzalloc`, `thread_create` with arguments and IDs, idle thread, wait queues,
sleep, semaphores, mutexes, reaper, input events + set 2 keyboard decoder.

### Step 4 — User mode (next)

`address_space_t` (builds on `mmu_root`), ring-3 GDT segments + TSS `rsp0` update on switch,
`int 0x80` syscalls (`IDT_GATE_USER_INTERRUPT`), first user program, user faults kill the process.

### Console / keyboard layer (can come before or alongside step 4)

- Move `driver/ps2.c` / `ps2.h` into `platform/pc`.
- Console layer: keymap (US, ABNT2) from keycodes to characters, modifiers / Caps Lock, line editing
  (echo, backspace, line buffering), blocking `console_read()`. Basis for the shell and `read()`.
- Thread-safe output (mutex around the tty) replacing direct `kprintf` from many threads.

### Optional — second arch skeleton

A stub `riscv64`/`qemu-virt` (or custom CPU) target implementing the contracts with stubs and a UART
`screen` backend. Link errors reveal remaining x86 assumptions.

## Known technical debt

- `page_unmap` does not reclaim empty intermediate tables.
- Direct map covers only the first 1 GiB; RAM above it is ignored (clamped in `mmap_init`).
- `MMU_EXEC` ignored: NX not enabled (`EFER.NXE`), every mapping is executable.
- Slab pages are never returned to the buddy allocator (no empty-slab reclamation, no cache destroy).
- `kmalloc` above 1024 B rounds up to a power-of-two number of pages (internal waste).
- `frame_free`/`frame_lookup` walk the zone list linearly.
- No SMP; `irq_save` is the only synchronization (becomes spinlocks + `irq_save` on SMP).
- `kprintf`/tty are not thread-safe: concurrent prints can interleave or corrupt the cursor.
- No `in_interrupt()` guard: blocking from an IRQ handler is not detected.
- No `thread_join`; a `thread_t *` from `thread_create` is only valid until that thread exits.
- Sleep queue is scanned linearly on every tick.
- Every block/yield goes through `int`/`iretq`; consider the two-level (Linux/xv6) context switch
  if blocking becomes frequent (hidden behind `arch_yield`).
- `ps2.h` declares `static` device state in a header, duplicated in every includer.
- `driver/ps2.c` / `driver/ps2.h` are PC-only code in the generic `driver/` dir.
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
