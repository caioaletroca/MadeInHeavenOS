# Session Handoff — MadeInHeavenOS

## Current state

Working x86-64 PC kernel with:

- Docker generic → custom MiHOS cross-toolchain pipeline (build only runs inside the Docker image)
- GRUB/Multiboot2 boot, higher-half kernel
- Physical memory map, zones, buddy allocator, early boot allocator
- `kmalloc`/`kfree` on slab size classes (16–1024 B) with buddy pages above that
- 4 KiB page map/unmap/translate behind an arch-neutral MMU contract
- Per-process address spaces: private user half, shared kernel half (`address_space_t`)
- Ring-3 GDT segments and a loaded TSS (RSP0 + IST stacks); identity map removed after boot
- User-mode threads (`thread_create_user`) preempted by the timer; `int 0x80` syscalls `write`,
  `read` and `exit` with user pointer validation
- PIC/PIT timer at 100 Hz, generic IRQ registration layer
- **Preemptive** round-robin scheduler (timer-driven), voluntary yield via software interrupt
- Dynamic kernel threads (`thread_create`), idle thread, reaper for exited threads
- Blocking primitives: wait queues, `thread_sleep`, semaphores, mutexes
- Generic input events; PS/2 scancode set 2 decoder feeding the console thread
- Console: US and ABNT2 keymaps with dead keys, line discipline, blocking `console_read()`
- UTF-8 tty drawing CP437 glyphs on VGA text mode with a hardware cursor; `kprintf` is atomic per call
- Kernel split into generic code, arch contracts and platform contracts, enforced by the build
- MM, preemption and sync self-tests passing in Bochs; keyboard verified in Bochs and QEMU

Latest commits:

```text
c0973c8 feat(sys): add int 0x80 syscalls with write, read and exit
d8dd7d8 feat(sched): run threads in user mode
3ed7197 docs(architecture): document address spaces, TSS and buddy fix in handoff
5ebcf13 feat(mm): add per-process address spaces
c9d7865 feat(libc): add memcmp
abaadbc fix(mm): stop toggle_bit truncating bitmap bits to int
27f1631 feat(arch): add user segments and load the TSS
33d2a0a fix(boot): actually remove the identity map
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
│   │   └── mmu.h                 #   arch_mmu_init/kernel_root/map/unmap/translate,
│   │                             #   arch_mmu_root_create/destroy/activate, MMU_* flags
│   ├── platform/platform.h       # contract every board implements
│   ├── boot_info.h               # protocol-independent boot data
│   ├── unicode.h                 # codepoint_t
│   ├── driver/                   # input, keymap, console, tty, screen, timer
│   └── sched/ mm/ ...
├── arch/x86/
│   ├── include/asm/              # PUBLIC arch interface: irq_flags.h cpu.h memory.h
│   ├── common/include/x86/       # PRIVATE: cpu gdt idt io isr paging tss vectors exceptions
│   └── x86_64/                   # boot, ISR stubs, gdt64.S, tss.c, arch.c, context.c, isr.c, paging.c
├── platform/pc/                  # pic, pit, ps2, keyboard, vga, screen (CP437), multiboot2, platform.c
├── sched/ mm/ driver/ sys/ selftest/
└── kmain.c
```

### Include rules (enforced in `kernel/Makefile`)

| Code in | Can include |
|---|---|
| every directory | `include/` and `<asm/...>` |
| `arch/` and `platform/` only | additionally `<x86/...>` |

A generic file including `<x86/...>` fails to compile. Inline asm must not appear outside `arch/`.
Platform-private headers (`pic.h`, `vga.h`, `ps2.h`, `keyboard.h`, `multiboot2.h`) are quote-included
from the same directory.

### Arch contract (what a new arch must provide)

| Piece | x86_64 implementation |
|---|---|
| Entry → `kmain(uintptr_t boot_handoff)` | `boot.S`, `boot64.S` (passes Multiboot2 address in `%rdi`) |
| Linker script defining `_kernel_physical_end` | `x86_64/linker.lds` |
| `<asm/memory.h>`: `PAGE_SIZE`, `KERNEL_VIRTUAL_ADDRESS`, `KERNEL_DIRECT_MAP_SIZE`, `KERNEL_SELFTEST_VIRTUAL_BASE`, `USER_BASE`, `USER_STACK_TOP`, `USER_TOP` | `arch/x86/include/asm/memory.h` |
| `<asm/irq_flags.h>`: `irq_save/restore/enable/disable/enabled`, `irq_flags_t` | inline `pushfq/cli/sti` |
| `<asm/cpu.h>`: `cpu_idle/cpu_halt/cpu_breakpoint` | inline `hlt`, Bochs magic breakpoint |
| `arch_init()` | `arch.c`: TSS, IDT, exceptions, page fault handler, yield vector |
| `arch_thread_context_init()`, `arch_yield()` | `context.c` + `thread_entry.S` trampoline |
| `irq_register()` + dispatch to `scheduler_on_interrupt()` | `isr.c` |
| `arch_mmu_*` (incl. root create/destroy/activate) | `paging.c` (`struct mmu_root { top, top_physical }`) |
| Toolchain flags | `config/<ARCH>.mk` (`ARCH_CFLAGS`) |

### Platform contract

`platform_boot_info_init(handoff, info)`, `platform_init()`, `platform_timer_init(hz)`,
`platform_irq_enable(irq)`, `platform_irq_eoi(irq)`, plus a `screen` backend.

### Boot sequence (`kmain`)

```text
tty_init → platform_boot_info_init → arch_init [TSS first] → input_init → platform_init (PIC, PS/2, keyboard IRQ)
→ mm_init(&boot_info) [mmap_init + kmalloc_init + arch_mmu_init] → scheduler_init [idle thread]
→ threads_init [reaper] → console_init(&keymap_abnt2) [console thread]
→ timer_init(TIMER_FREQUENCY_HZ) → irq_enable → mm_selftest → scheduler_selftest [+ sync tests]
→ user_selftest → boot thread idles
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
- **Zones:** `mmap_split_region` cuts every available region into power-of-two zones (largest first,
  remainders under 16 frames are dropped). `frame_alloc` tries zones newest-first, i.e. the small
  zones at the top of RAM. The buddy bitmap holds one bit per pair (XOR of both halves' free state).

### Virtual memory and address spaces

```text
L4[0..255]   user half    private per address space, every page owned by it
L4[256..511] kernel half  shared: arch_mmu_init gives every entry an L3 table at boot,
                          arch_mmu_root_create copies the 256 entries into each new root
```

- The boot identity map (`L4[0]`) is removed in `boot64_high`, after GDTR is reloaded with the GDT's
  virtual address (`gdt64_pointer_high`). Low physical memory is reachable only through the direct map.
- **User layout** (`<asm/memory.h>`): `USER_BASE` 4 MiB (page 0 stays unmapped for NULL),
  `USER_STACK_TOP` `0x00007FFFFFFFF000`, `USER_TOP` `0x0000800000000000` (canonical limit).
- `address_space_t` (`mm/address_space.c`): `create`, `map` (fresh zeroed frames, `MMU_USER` added),
  `write` (copies through the direct map, no activation needed), `activate` (NULL = kernel root),
  `destroy`. **Ownership rule:** everything mapped in the user half is freed with the space, so shared
  or kernel frames must never be mapped there.
- `arch_mmu_root_destroy` walks `L4[0..255]` and frees pages and tables; it panics on the active root.
  `arch_mmu_activate` skips the CR3 reload when the root is already active.
- Intermediate tables get the USER bit when the mapping has `MMU_USER` (`page_table_next`); the kernel
  half's L4 entries never do, so ring 3 cannot reach it. SMAP is off, so the kernel reads user pages
  directly.

### GDT and TSS (`gdt64.S`, `tss.c`)

| Index | Selector | Descriptor |
|---|---|---|
| 1 | `0x08` | kernel code |
| 2 | `0x10` | kernel data |
| 3 | `0x1B` | user data (DPL 3) |
| 4 | `0x23` | user code (DPL 3) |
| 5–6 | `0x28` | TSS (16-byte system descriptor) |

- User data precedes user code because `sysret` loads SS = base + 8 and CS = base + 16.
- `tss_init()` (first thing in `arch_init`) fills the TSS descriptor base, sets IST1/IST2 to the top of
  their stacks (no IDT gate uses them yet), `iomap_base = sizeof(tss)` (no ring-3 port I/O), and runs
  `ltr`. `tss_set_kernel_stack(top)` sets RSP0, the stack the CPU loads on an interrupt from ring 3.

### User mode and syscalls

- `thread_create_user(space, entry, user_stack)`: kernel stack + `arch_user_context_init` frame
  (`cs = 0x23`, `ss = 0x1B`, `RFLAGS_USER_THREAD` = IF, IOPL 0). Entering ring 3 is just the normal
  `isr_common` → `iretq`. `thread_t.space` is NULL for kernel threads; the space is not owned by the
  thread.
- On every switch `scheduler_on_interrupt` calls `arch_thread_switch` (RSP0 = top of the kernel stack,
  user threads only) and `address_space_activate(next->space)` (kernel threads run on the kernel root,
  so a dead process's root is never active when it is destroyed).
- **Syscall ABI** (`include/syscall.h`, shared with assembly via `__ASSEMBLER__`): `int $0x80`
  (DPL 3 interrupt gate, IRQs off on entry), `rax` = number, `rdi rsi rdx r10 r8 r9` = arguments,
  `rax` = result or `-errno` (Linux values). `r10` instead of `rcx` keeps the ABI valid for a later
  `syscall`/`sysret`.
- `SYS_EXIT` 0, `SYS_WRITE` 1 (fd 1/2 → console, 256-byte chunks), `SYS_READ` 2 (fd 0, one line).
  Generic `sys/syscall.c` → `syscall_dispatch`; x86 `syscall.c` unpacks the frame.
- User memory is only touched through `address_space_read/write` (direct map, user half, mapped
  pages only), so kernel pointers fail with `-EFAULT` instead of leaking.
- `exit` and blocking `read` work from inside a syscall: `int 0x81` nests a second frame on the
  thread's kernel stack.
- Self-tests (`selftest/user_program.S`, `selftest/user.c`): position-independent programs copied to
  `USER_BASE` — a counter loop that exits when the kernel sets its `stop` byte, and a hello that also
  checks a kernel-pointer `write` returns `-EFAULT`.

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

## Input and console

```text
IRQ1 → platform/pc/keyboard.c (set 2 decoder) → input_report_key(keycode, pressed)
     → driver/input.c ring (128 events) → console thread (only consumer of input_get_event)
     → keymap_translate → dead keys / Ctrl → line discipline → console ring → console_read()
                                                            └→ echo → tty_write
```

- `keycode_t` (`include/driver/input.h`) names physical key positions on a US layout, not characters
  (`KEY_SEMICOLON` types 'ç' on ABNT2). Covers 104 keys, ABNT2 extras (`KEY_102ND`, `KEY_RO`,
  `KEY_KP_COMMA`), keypad, media and ACPI keys. Full input buffer drops events.
- Decoder handles `E0` extended keys, `F0` releases, Print Screen fake shifts and the 8-byte Pause
  sequence (reported as press + release). Read the data port exactly once per IRQ.
- **Keymaps** (`driver/keymap.c`): `normal`/`shift`/`altgr` tables of `codepoint_t`. `keymap_translate`
  is stateless: it returns a code point, `KEYMAP_DEAD | accent` for a dead key, or 0. Level order
  AltGr → Shift → normal, each falling back when empty. Caps Lock flips Shift on lowercase letters
  only; keypad digits and `KP_DOT` type nothing with Num Lock off. `keymap_compose(accent, base)`
  looks up dead key compositions (`´a → á`, `~a → ã`, `´c → ç`).
- **Console** (`driver/console.c`): one thread owns all key state (`down[]`, Caps/Num Lock toggled on
  the press edge so typematic repeat is ignored, pending accent). The line discipline handles echo,
  backspace (removes a whole UTF-8 sequence, erases one cell with `"\b \b"`), Ctrl+U, Ctrl+C (drops
  the line, no signals yet) and Ctrl+D (EOF). Tab, Esc and arrows are ignored.
- **Console ring** (xv6 / Linux `n_tty` style): `read_index ≤ write_index ≤ edit_index`, free-running
  indices over a power-of-two buffer. Readers consume `[read, write)`, the console thread edits
  `[write, edit)`. Ordinary input leaves one byte free so `\n`/EOF can always commit. `console_read`
  is canonical: at most one line per call; EOF is consumed only when it comes first, so the next
  read returns 0.
- **Output:** `tty_write` decodes UTF-8 (U+FFFD for malformed/overlong) and runs under `irq_save`,
  so each `kprintf` is atomic and callable from IRQs, panic and early boot. The PC `screen_glyph`
  maps code points to CP437 and falls back to the unaccented letter (`ã → a`).
- The console's read/write signatures already match `read`/`write`; in step 4 they become the first
  `file_ops_t` behind fds 0–2.

## Interrupt vectors (`x86/vectors.h`)

| Range | Use |
|---|---|
| 0x00–0x1F | CPU exceptions (page fault = 14) |
| 0x20–0x2F | PIC IRQs (timer = IRQ 0, keyboard = IRQ 1) |
| 0x80 | Syscalls (DPL 3 interrupt gate) |
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
- `ps2_interface_test` stored the second port's result in slot 0; `ps2.h` defined the device table
  `static` in every includer.
- VGA frame buffer was written through its physical address (leftover identity mapping).
- Debug code reading port `0x60` twice stole the next scancode byte, so releases decoded as presses.
- `boot64_high` used `movq page_table_l4, %rax` (loads the **contents**) instead of the address, so the
  identity map was never removed and the GDT only worked through it. `boot64` patched the TSS
  descriptor with the same mistake and never ran `ltr`; a 64-bit `lgdt` of a 4-byte-base pointer read
  garbage once data followed it.
- `toggle_bit` returned the masked `unsigned long` as `int`, truncating bits 32–63 to 0, so
  `buddy_free` merged with buddies still in use. Only zones with more than 32 pairs per order were hit
  (every zone but the four smallest), including during the boot release; it surfaced when 255 page
  tables pushed allocations past the small top zones. Found by replaying the allocation log through a
  Python port of `buddy.c`.
- `scheduler_selftest` was `noreturn` and ended in an idle loop, so later self-tests never ran.

## Recommended next steps

### Step 2 — Memory management debt (done)

Frame API on `physaddr_t`, `mmap_release_regions` fix, boot allocator replacing `KERNEL_HEAP_SIZE`,
per-frame ownership, on-slab headers, `kmalloc`/`kfree`.

### Step 3 — Threads and synchronization (done)

Allocator locking + `kzalloc`, `thread_create` with arguments and IDs, idle thread, wait queues,
sleep, semaphores, mutexes, reaper, input events + set 2 keyboard decoder.

### Step 4 — User mode (in progress)

- **4a (done):** ring-3 GDT segments, TSS loaded from C, identity map really removed.
- **4b (done):** `address_space_t`, shared kernel half, root create/destroy/activate.
- **4c (done):** user threads, RSP0/CR3 switch hook, timer preempts ring 3.
- **4d (done):** `int 0x80` syscalls `write`/`read`/`exit`, user pointer validation.
- **4e (next):** in the exception and page fault handlers, `(cs & 3) == 3` prints the fault and
  calls `thread_exit` instead of `panic`. Test: a user program that reads address 0 or runs `hlt`;
  the kernel reports it and keeps running. Later the reaper destroys the address space (never the
  active one).
- **4f:** `process_t` with address space and fd table; file layer (`file_t` + `file_ops_t { read,
  write }`, console as fds 0–2); ELF loader from a GRUB Multiboot2 module; userland libc (`crt0`,
  syscall stubs, `FILE` over `fd`, built without `-mcmodel=kernel`).

### Console / keyboard layer (done)

PS/2 moved into `platform/pc`, UTF-8 tty with CP437 glyphs and hardware cursor, atomic `kprintf`,
US/ABNT2 keymaps with dead keys, line discipline, blocking `console_read()`.

### Optional — second arch skeleton

A stub `riscv64`/`qemu-virt` (or custom CPU) target implementing the contracts with stubs and a UART
`screen` backend. Link errors reveal remaining x86 assumptions.

## Known technical debt

**Open bugs (fix first):**

- `console_store` indexes with `% (CONSOLE_BUFFER_SIZE - 1)` while every other access uses
  `% CONSOLE_BUFFER_SIZE`: reads and writes diverge after 1023 bytes of total input.
- `console_read` checks `CTRL('C')` instead of `CTRL('D')` for EOF: EOF never ends a read and `0x04`
  is returned as data.

**Debt:**

- `page_unmap` does not reclaim empty intermediate tables.
- Direct map covers only the first 1 GiB; RAM above it is ignored (clamped in `mmap_init`).
- `MMU_EXEC` ignored: NX not enabled (`EFER.NXE`), every mapping is executable.
- Slab pages are never returned to the buddy allocator (no empty-slab reclamation, no cache destroy).
- `kmalloc` above 1024 B rounds up to a power-of-two number of pages (internal waste).
- `frame_free`/`frame_lookup` walk the zone list linearly.
- No SMP; `irq_save` is the only synchronization (becomes spinlocks + `irq_save` on SMP).
- No `in_interrupt()` guard: blocking from an IRQ handler is not detected.
- No `thread_join`; a `thread_t *` from `thread_create` is only valid until that thread exits.
- Sleep queue is scanned linearly on every tick.
- Every block/yield goes through `int`/`iretq`; consider the two-level (Linux/xv6) context switch
  if blocking becomes frequent (hidden behind `arch_yield`).
- `<asm/...>` lives at family level (`arch/x86/include`) but uses x86_64-only instructions.
- Kernel `install-headers` copies internal headers into the sysroot (stale copies can hide
  include errors → use a clean build after moving headers).
- `ARCH_CFLAGS` (`-mcmodel=kernel`) also applies to the future userland libc.
- `tty_write` holds IRQs off for the whole write plus a full 4000-byte VGA copy; long user writes
  will need a mutex and a separate panic path.
- Screen redraws the whole buffer on every write; scrolling uses `memcpy` on an overlapping range.
- Single tty UTF-8 decoder state; relies on every write going through the IRQ-off section.
- Keyboard LEDs never follow Caps/Num Lock (`0xED` not sent; needs a PS/2 command path).
- No raw mode / termios; Tab, Esc, arrows and Home/End are ignored in line editing.
- Second-port interface test runs even when no second port was detected (may hang on
  single-channel controllers).
- `vsnprintf` has no precision (`%.*s` panics) and `%s` does not bound-check `remain`.
- libc `FILE`: `stderr` points past `stdio_streams[2]`, `fputc` writes through a NULL buffer,
  `fwrite` does not reset its per-item counter.
- Bochs (win32 GUI) sends Left Ctrl for the ABNT2 `/ ?` key, so `KEY_RO` can only be tested on QEMU.
- No IDT gate uses the IST stacks yet; a double fault on a broken kernel stack still triple-faults.
- `mmap_split_region` drops region remainders smaller than 16 frames.
- `address_space_map` leaves already-mapped pages in place when it fails midway (freed on destroy).
- Any fault in user mode still panics the kernel (step 4e).
- Syscalls run with IRQs off for their whole duration (interrupt gate); long writes delay IRQs.
- The user self-tests leak their two address spaces: without a join or process object nothing knows
  when the thread is gone.
- In the counter test the counter shares a cache line with the loop's code, so every increment is
  handled as self-modifying code (about 25× slower than with the counter in a separate line).
- The buddy allocator has no self-test; a randomized alloc/free check per zone size would have caught
  the `toggle_bit` truncation immediately.

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

Run QEMU (verifies keys Bochs drops; `cpu_breakpoint` is a no-op there):

```sh
qemu-system-x86_64 -cdrom mihos.iso -m 1024 -no-reboot -no-shutdown
qemu-system-x86_64 -cdrom mihos.iso -m 1024 -no-reboot -no-shutdown -d int,cpu_reset -D qemu.log
```

Local, intentionally uncommitted: `.bochsrc`, `bx_enh_dbg.ini`.
