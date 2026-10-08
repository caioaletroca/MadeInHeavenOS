# Session Handoff — MadeInHeavenOS

Where the project is going (end goal, phases, the POSIX/libc strategy, repository layout target):
see [`ROADMAP.md`](ROADMAP.md). This file describes where it is now.

## Current state

Working x86-64 PC kernel with:

- Docker generic → custom MiHOS cross-toolchain pipeline (build only runs inside the Docker image)
- GRUB/Multiboot2 boot, higher-half kernel
- Physical memory map, zones, buddy allocator, early boot allocator
- `kmalloc`/`kfree` on slab size classes (16–1024 B) with buddy pages above that
- 4 KiB page map/unmap/translate behind an arch-neutral MMU contract
- Per-process address spaces: private user half, shared kernel half (`address_space_t`)
- Ring-3 GDT segments and a loaded TSS (RSP0 + IST stacks); identity map removed after boot
- User-mode threads preempted by the timer; `int 0x80` syscalls `write`, `read`, `close` and `exit`
  with user pointer validation
- Processes (`process_t`) owning their address space and fd table, waitable, refcounted; faults in
  ring 3 kill the process with 128 + signal instead of panicking
- Open files (`file_t` + `file_ops_t`), console as the first backend behind fds 0–2
- **ELF64 loader**: user programs built separately (`user/`), loaded by GRUB as Multiboot2 modules,
  validated and mapped by `elf_load`, started by `init_start` (`hello` exits with 42)
- **uapi headers** (`<mihos/syscall.h>`, `<mihos/errno.h>`, `<mihos/signal.h>`): the only kernel
  headers installed into the sysroot
- **Own libc**: `libk.a` (kernel) and `libc.a` + `crt0.o` (user) from one tree; inline-asm syscall
  layer, `errno`, `write`/`read`/`close`/`_exit`, `exit`/`abort`; buffered stdio over fds with
  `printf` (output side; input side next)
- PIC/PIT timer at 100 Hz, generic IRQ registration layer
- **Preemptive** round-robin scheduler (timer-driven), voluntary yield via software interrupt
- Dynamic kernel threads (`thread_create`), idle thread, reaper for exited threads
- Blocking primitives: wait queues, `thread_sleep`, semaphores, mutexes
- `spinlock_t` (IRQs off + misuse checks on one CPU) and scope guards (`guard()`/`scoped_guard()`
  in `cleanup.h`); sleeping with a lock held goes through `wait_queue_sleep_locked`
- Generic input events; PS/2 scancode set 2 decoder feeding the console thread
- Console: US and ABNT2 keymaps with dead keys, line discipline, blocking `console_read()`
- UTF-8 tty drawing CP437 glyphs on VGA text mode with a hardware cursor; `kprintf` is atomic per call
- Kernel split into generic code, arch contracts and platform contracts, enforced by the build
- Guard, MM, preemption, sync, console, user-mode and ELF self-tests passing in Bochs; keyboard
  verified in Bochs and QEMU

Latest commits:

```text
0031357 feat(libc): add ferror
335c05d docs(architecture): add roadmap and update handoff for libc
f7e1942 feat(libc): add buffered stdio over file descriptors
02e12fd feat(libc): build libc.a for user programs with syscall wrappers
3579938 refactor(build): install only the uapi headers into the sysroot
30eb5b8 docs(architecture): document locking, guards and the ELF loader in handoff
cd4c689 feat(exec): load ELF programs and start the first one
ea14a19 build(user): build user programs as GRUB modules
d824c15 feat(boot): record bootloader modules and keep them reserved
1d3b986 feat(libc): add strlcpy and strcmp, terminate strcpy
cd7b843 refactor(driver): protect the console ring with a spinlock
8f5f1d8 refactor(sched): put semaphores, mutexes and thread lists on spinlocks
```

## Architecture

### Layers

```text
libc/                             # libk.a (kernel) + libc.a + crt0.o (user); see "Userland libc"
user/                             # user programs: user.ld, one directory per program
grub/                             # grub.cfg + ISO build (kernel + modules)
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
│   ├── cleanup.h                 # guard() / scoped_guard() mechanism
│   ├── exec/                     # elf.h (ELF64 format), exec.h
│   └── sched/ mm/ fs/ ...        # sched/spinlock.h: spinlock_t + irq/spinlock guards
├── arch/x86/
│   ├── include/asm/              # PUBLIC arch interface: irq_flags.h cpu.h memory.h elf.h
│   ├── common/include/x86/       # PRIVATE: cpu gdt idt io isr paging tss vectors exceptions
│   └── x86_64/                   # boot, ISR stubs, gdt64.S, tss.c, arch.c, context.c, isr.c, paging.c
├── platform/pc/                  # pic, pit, ps2, keyboard, vga, screen (CP437), multiboot2, platform.c
├── sched/ mm/ driver/ sys/ fs/ exec/ selftest/
├── init.c                        # init_start: runs the first user program
└── kmain.c
```

### Build order

```text
headers (kernel uapi → sysroot) → libc (libk.a, libc.a, crt0.o) → kernel → user → grub (ISO)
```

The kernel includes its uapi headers through `-Iinclude/uapi`; `install-headers` copies only
`include/uapi/`, so user code and libc see `<mihos/...>` and never kernel internals. Kernel code uses
`CFLAGS` (`ARCH_CFLAGS`: `-mcmodel=kernel -mno-red-zone`); ring-3 code (libc.a, `user/`) uses
`USER_CFLAGS` from `common.mk` (freestanding, no PIE, small code model, no unwind tables).

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
| `<asm/elf.h>`: `ELF_MACHINE` (the ELF `e_machine` this arch runs) | `62` (`EM_X86_64`) |
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
→ timer_init(TIMER_FREQUENCY_HZ) → irq_enable → guard_selftest → mm_selftest
→ scheduler_selftest [+ sync tests] → console_selftest → user_selftest → elf_selftest(&boot_info)
→ init_start(&boot_info) [runs the 'hello' module, waits] → boot thread idles
```

## Memory management

```text
boot_info regions ──▶ mmap_init ──▶ zones (one buddy system each) ──▶ frame_alloc/free (physaddr_t)
                        │                                                  │
                        └─ boot_alloc: zone_t, frame_t[], bitmaps          ├─▶ slab caches ─▶ kmalloc ≤ 1024 B
                           (sealed by kmalloc_init)                        └─▶ kmalloc > 1024 B (2^order pages)
```

- **Physical layout:** `[kernel image][boot modules][boot metadata][usable memory …]`. The boot
  metadata reservation is computed from the memory map (`mmap_metadata_size`), not a fixed size. Only
  memory inside `KERNEL_DIRECT_MAP_SIZE` (1 GiB) is handed to zones.
- **Boot modules:** GRUB puts modules right after the kernel, in memory the firmware map reports as
  available. `mmap_init` places the metadata after the kernel *and* every module (`image_end`), and
  everything below `usable_start` stays reserved, so modules are never handed out as free frames.
  A module ending above the direct map panics (`phys_to_kern` could not reach it).
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

- `thread_create_user(process, entry, user_stack)` (called only by `process_start`): kernel stack +
  `arch_user_context_init` frame (`cs = 0x23`, `ss = 0x1B`, `RFLAGS_USER_THREAD` = IF, IOPL 0).
  Entering ring 3 is just the normal `isr_common` → `iretq`. The thread gets `process` and caches
  `process->space` in `thread_t.space` for the switch; both are NULL for kernel threads.
- On every switch `scheduler_on_interrupt` calls `arch_thread_switch` (RSP0 = top of the kernel stack,
  user threads only) and `address_space_activate(next->space)` (kernel threads run on the kernel root,
  so a dead process's root is never active when it is destroyed).
- **Syscall ABI** (`include/syscall.h`, shared with assembly via `__ASSEMBLER__`): `int $0x80`
  (DPL 3 interrupt gate, IRQs off on entry), `rax` = number, `rdi rsi rdx r10 r8 r9` = arguments,
  `rax` = result or `-errno` (Linux values). `r10` instead of `rcx` keeps the ABI valid for a later
  `syscall`/`sysret`.
- `SYS_EXIT` 0 (`process_exit`), `SYS_WRITE` 1 (256-byte chunks), `SYS_READ` 2 (one chunk),
  `SYS_CLOSE` 10. Read and write look the fd up in the process's table and call the file's ops;
  an empty slot, an out-of-range fd or a missing `FILE_READ`/`FILE_WRITE` flag gives `-EBADF`.
  Generic `sys/syscall.c` → `syscall_dispatch`; x86 `syscall.c` unpacks the frame.
- User memory is only touched through `address_space_read/write` (direct map, user half, mapped
  pages only), so kernel pointers fail with `-EFAULT` instead of leaking.
- `exit` and blocking `read` work from inside a syscall: `int 0x81` nests a second frame on the
  thread's kernel stack.
- **User faults** (`exceptions.c`, `paging.c`): when the saved CS has RPL 3 (`isr_from_user`, in
  `x86/isr.h`), the handler prints the fault and calls `process_exit(SIGNAL_EXIT_STATUS(sig))`;
  in ring 0 it still panics. NMI, `#DF` and `#MC` always panic. `exception_signals[]` follows Linux:
  `#DE`/x87/SIMD → SIGFPE, `#DB`/`#BP` → SIGTRAP, `#UD` → SIGILL, `#NP`/`#SS`/`#AC` → SIGBUS, the
  rest (incl. `#GP`, `#PF`) → SIGSEGV, unlisted → SIGKILL. Returning instead would re-run the
  faulting instruction forever.
- Self-tests (`selftest/user_program.S`, `selftest/user.c`): position-independent programs copied to
  `USER_BASE`, each started as a process with the console on fds 0–2, waited for, checked through
  its variables and exit status, then released:
  - `loop`: counts until the kernel sets `stop`, exits 0 (preemption of ring 3)
  - `hello`: prints, checks a kernel-pointer `write` returns `-EFAULT`, exits with it
  - `null` / `hlt`: set `before`, fault (`#PF` / `#GP`), must never set `after`; status 139
  - `files`: `-EBADF` for empty, out-of-range and closed fds and double close; stdout still works
    after `close(2)`

### Processes (`sched/process.c`)

```text
process_create(space) ──▶ process_fd_install(p, file) ×N ──▶ process_start(p, entry, stack)
     refs = 1 (handle)                                          refs++ (thread), then the thread runs
```

- `process_t`: pid, state (`RUNNING`/`EXITED`), `exit_status`, `refs`, `space`, `thread` (one per
  process), `waiters`, `files[16]`. Allocated with `kzalloc`.
- **Ownership:** the process owns its space (destroyed with it) and one reference on each file in its
  table. `process_fd_install` takes the caller's file reference on success (lowest free fd, POSIX);
  on `-EMFILE` the caller keeps it.
- Create and start are split so fds are installed before the program can run. `process_start` takes
  the thread's reference *before* creating the thread (it can exit before `thread_create_user`
  returns) and drops it again if creation fails; the process is then still valid and not running.
- `process_exit(status)`: records the status, closes every fd (at exit, not release, so pipes can see
  EOF later), wakes waiters, `thread_exit()`. It never touches the space: this thread still runs on it.
- The **reaper** drops the thread's reference after freeing stack and struct. `process_release` at
  `refs == 0` closes any remaining fds (a process that never ran), destroys the space and frees the
  struct; only the decrement runs with IRQs off. The last release is always on a kernel thread, so the
  space is never active when destroyed.
- `process_wait(p)`: Mesa loop on `waiters` until `EXITED` (`wait_queue_sleep_locked` on `p->lock`),
  returns `exit_status`. The struct and space stay valid until the caller's `process_release`, so tests
  can read the program's variables.
- **Locking:** `p->lock` protects `refs`, `state` and `exit_status`; `process_exit` sets the status
  and wakes waiters under it. The last release and the teardown run *after* the lock is dropped, never
  freeing a held lock. pids come from `pid_lock`.
- **Exit status:** the value passed to `exit` (full `int`, no `& 0xFF` until `waitpid` exists), or
  128 + signal for faults (`include/signal.h`: Linux signal numbers, no delivery yet).
- The fd table has no lock: with one thread per process only the creator (before start) and the
  process itself touch it. Needs one with threads or `fork`.

### Files (`fs/file.c`)

```text
process->files[fd] ──▶ file_t { ops, flags, refs, private } ──▶ ops->read / write / release
```

- An fd is an index into the process's table; a `file_t` is an open file (POSIX open file
  description) that several fds can share. `file_ops_t` takes **kernel** buffers and returns bytes or
  `-errno`; the syscall layer does every user copy, so backends never see user pointers.
- `file_create(ops, flags, private)` (refs = 1), `file_get`, `file_put` (at 0: optional `release`,
  then `kfree`). Refs are protected by the file's embedded `lock`; `release` runs without locks held,
  once, after the last reference is gone. No offset yet: nothing seeks until a VFS exists.
- **Console backend** (`driver/console.c`): one `FILE_READ | FILE_WRITE` file created in
  `console_init`, which keeps its own reference forever. `console_file()` returns a new reference;
  user programs get it three times, as fds 0, 1 and 2 (like `getty` opening the tty and `dup`ing it).

### Programs and the ELF loader (`exec/`, `init.c`, `user/`)

```text
user/hello ─build─▶ sysroot/usr/bin/hello.elf ─ISO─▶ GRUB module2 "hello" ─▶ boot_info.modules[]
init_start ─▶ exec_load(image, size) ─▶ address_space_create + elf_load + stack page
           ─▶ process_create + console fds 0-2 + process_start(entry) ─▶ process_wait
```

- **User build** (`user/Makefile`): one static ELF per program directory, linked as
  `crt0.o` (first: holds `_start`) + the program's objects + `-lc -lgcc`, with `USER_CFLAGS` and
  `max-page-size=4096`. `user.ld` puts text (RX), rodata (R) and data+bss (RW) in separate `PHDRS`
  segments, each on its own page at `USER_BASE`. Installed to `sysroot/usr/bin`, copied into the
  ISO by `grub/Makefile` and named in `grub.cfg` (`module2 /boot/hello.elf hello`).
- **Modules** (`boot_info.modules[]`, Multiboot2 tag 3): physical `[start, end)` plus the name, copied
  with `strlcpy` (the Multiboot2 structure is not protected after boot).
- **`elf_load(space, image, size, &entry)`** (`exec/elf.c`): validates `ELF64`, little-endian,
  version, `ET_EXEC`, `e_machine == ELF_MACHINE` and that the program header table is inside the
  image; for every non-empty `PT_LOAD` (empty ones, e.g. an unused RW segment, are skipped) checks the
  file range and that memory lies in `[USER_BASE, USER_TOP)`, maps whole pages (`PF_W`/`PF_X` →
  `MMU_WRITE`/`MMU_EXEC`; readable is implied) and copies `p_filesz` bytes to `p_vaddr` (`.bss` stays
  zero: frames are zeroed). The entry must be inside an executable segment. Returns `-ENOEXEC` /
  `-ENOMEM`, sets `entry` only on success, never creates a process. On error the caller destroys the
  space. Every bound is a subtraction after a `<=` check; `p_vaddr > USER_TOP` is rejected **before**
  `USER_TOP - p_vaddr`, which would wrap and let a segment map into the kernel half.
- **`exec_load(image, size, &space, &entry)`** (`exec/exec.c`): new space + `elf_load` + one stack page
  at `USER_STACK_TOP - PAGE_SIZE`; frees everything on error. Meant for `sys_exec`/spawn later too.
- **`init_start(info)`** (`init.c`): finds the `hello` module, `exec_load`, process with the console on
  fds 0–2, start, wait, report, release. Later: start `/sbin/init` and treat its exit as fatal.
- `hello` returns `status + zero` from a `.data` 42 and a `.bss` 0, so its exit status checks the copy
  and the zeroing.
- **ELF self-test** (`selftest/elf.c`): `exec_load` of the module must succeed with the header's entry;
  ten broken copies (magic, class, machine, `ET_DYN`, truncated, program headers out of bounds,
  `p_filesz > p_memsz`, segment in the kernel half, segment wrapping past 2^64, entry outside every
  segment) must return `-ENOEXEC` without setting the entry.

### Userland libc (`libc/`)

Our own libc until the start of roadmap phase D, then mlibc in user space (see `ROADMAP.md`).

- **Two libraries, one tree** (`libc/Makefile`): `libk.a` (kernel flags, `-D__is_libk`) and `libc.a`
  (`USER_CFLAGS`, `-D__is_libc`), each with its own object tree. In `subdir.mk`, `local_sources` go
  into both, `hosted_local_sources` (syscalls, stdio streams, `errno`, `exit`) only into `libc.a`.
  `libk.a` = `mem*`/`str*` (incl. `strlcpy`, `strcmp`), `vsnprintf`, `abs`/`atoi`.
- **`crt0.o`** (`libc/arch/x86_64/crt0.S`) is built and installed on its own, not inside `libc.a`
  (archive members are only pulled for undefined symbols, and nothing references `_start`):
  `xor %ebp`, `call main`, `call exit` with the return value. Assembly so `main` gets the SysV
  `RSP + 8` alignment.
- **Private headers** (never installed, `-Iinternal -Iarch/$(ARCH)`): `internal/syscall.h`
  (`__syscall(...)` counts its arguments and picks `__syscallN`; `syscall(...)` turns `-errno` into
  `-1` + `errno` via `__syscall_ret`, Linux convention `-4095..-1`) and `arch/x86_64/syscall_arch.h`
  (inline asm per argument count: `int $0x80`, `rdi rsi rdx`, `r10 r8 r9` bound with register
  variables, only a `"memory"` clobber since the kernel preserves every register but `rax`; cast
  macros named like the functions so pointers can be passed). `internal/FILE.h` holds stdio's
  internals.
- `errno` is a plain global (thread-local once user threads exist). `unistd`: `write`, `read`,
  `close`, `_exit`, one function per file. `exit` flushes stdio (`__stdio_exit`) then `_exit`;
  `abort` is `_exit(134)` (128 + SIGABRT, no signals yet).
- **stdio** (output side done): `FILE` is opaque in `<stdio.h>`; `struct _FILE` has `fd`, `flags`
  (`F_READ/F_WRITE`, `F_EOF/F_ERR` indicators, `F_LINEBUF/F_NOBUF`), `mode` (none/read/write: what
  the one buffer holds), `buffer`/`size`/`pos`/`len`, `ungot`, `next` (list of open streams from
  `__stdio_head`). Static `BUFSIZ` buffers: `stdin` and `stdout` line-buffered (the console is
  interactive; no `isatty` yet), `stderr` unbuffered. `__fwritex` is the only code that fills the
  output buffer: pending bytes always leave first; line-buffered streams flush up to the last `'\n'`;
  unbuffered or oversized data is written directly in one go. `__write_all` loops over short writes
  (`<= 0` → `F_ERR`), `__fflush_one` writes pending output and empties the buffer (dropped on error).
  `fflush(NULL)` walks the stream list. `ferror` reads `F_ERR`. Public functions are thin wrappers: `fputc`, `fputs`,
  `puts`, `putchar`, `fwrite`, `printf`/`fprintf`/`vprintf`/`vfprintf` (formats into a `BUFSIZ`
  stack buffer, then one `__fwritex`).

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
- Ready queue, sleep queue, wait queue lists and each thread's `state`/`run_link`/`wake_tick` are
  scheduler state protected by **IRQs off on purpose** (locking note at the top of `scheduler.c`): a
  spinlock there would be held across the context switch. SMP needs per-CPU run queues and a lock
  handed over the switch (Linux's `rq->lock` / `finish_task_switch`). IRQ gates 32–47 are interrupt
  gates (IF cleared).
- EOI is sent before switching threads.

### Threads

- `thread_create(entry, arg)`: `kzalloc`'d `thread_t` + 16 KiB `kmalloc`'d stack, flagged
  `THREAD_FLAG_OWNED`. `thread_init(...)` sets up threads on caller-owned memory (idle, tests).
- IDs come from a global counter under `tid_lock` (boot thread = 0).
- `thread_t` has no lock of its own: its fields are either set once before the thread is visible or
  scheduler state, protected by the lock of the list the thread is on.
- `entry(arg)` returning → `thread_start` → `thread_exit()`.
- **Invariant:** `run_link` is in exactly one list — ready queue, a wait queue, the sleep queue or
  the zombie list — or none (the running thread, exited static threads).
- **Idle thread** (static stack, `cpu_idle` loop) runs only when nothing is ready; never queued.
- **Reaper:** `thread_exit` disables IRQs for good, marks the thread `TERMINATED`, and (if owned)
  puts it on `zombie_list` (under `zombie_lock`, raw acquire/release since IRQs are never restored)
  + `semaphore_up(zombie_count)`, then yields. The reaper thread sleeps on that semaphore, unlinks
  the zombie under `zombie_lock` and frees stack + struct after it, then drops the process reference.
  IF=0 guarantees the exiting thread is off its stack first.

### Locks and guards (`sched/spinlock.h`, `cleanup.h`)

- **`spinlock_t`** (`bool locked`): on one CPU, IRQs off is the exclusion; `locked` only catches
  misuse. `spinlock_irqsave` (returns the flags) / `spinlock_irqrestore`; underneath,
  `spinlock_acquire`/`release` take and drop the lock without touching IRQs and panic if called with
  IRQs enabled. Panics on taking a held lock (recursion, or an IRQ handler taking its own thread's
  lock: a deadlock on SMP) and on a double unlock. On SMP `locked` becomes the atomic lock word behind
  the same API. No plain `spin_lock` yet: without a preemption counter it would not stop the timer.
- **Guards** (`cleanup.h`, on GCC's `cleanup` attribute): `guard(name, args...)` holds until the end
  of the enclosing scope (C# `using var`), `scoped_guard(name, args...) { ... }` for one block (C#
  `lock`). Released on every exit (end, `return`, `break`, `goto` out), in reverse order. A guard is
  `name##_guard_t` + `name##_guard_init(args)` + `name##_guard_exit(state *)`, defined next to the
  resource: `guard(irq)` and `guard(spinlock, &lock)`. Pitfalls: inside `scoped_guard`, `break`/
  `continue` leave the hidden `for`; always use braces (a stray `;` after `scoped_guard(...)` is an
  empty body); no `goto` into a guard's scope.
- **Rules:** a lock sits next to the data it protects, with a "protected by" comment. Never free an
  object while holding its embedded lock: decrement under the lock, tear down after it. `guard(irq)`
  is not mutual exclusion on SMP; shared data gets a named lock. Explicit acquire/release stay where
  the release is deliberately skipped (`thread_exit`) or moved early (`process_release`).

### Blocking and synchronization (`sched/wait.c`, `sched/sync.c`)

- `scheduler_block(list)`: current → `BLOCKED`, onto the list, `arch_yield()`. Must be called with
  IRQs off; returns after wake-up still with IRQs off (IF is saved per thread in its frame).
- `scheduler_wake(thread)`: `BLOCKED` → `READY` onto the ready queue; requests an immediate
  reschedule when the CPU is idling. Safe from IRQ handlers.
- **Never sleep holding a spinlock.** `wait_queue_sleep_locked(queue, lock)` releases the lock,
  blocks and retakes it before returning, all with IRQs off, so no wake-up slips between the check and
  the sleep. It is the only way to sleep on a wait queue (the unlocked `wait_queue_sleep` was removed).
  Usage (Mesa semantics: recheck in a loop):
  ```c
  scoped_guard(spinlock, &obj->lock)
  {
      while (!obj->condition)
          wait_queue_sleep_locked(&obj->waiters, &obj->lock);
  }
  // waker: set the condition and wake under the same lock
  ```
- `wait_queue_t`: FIFO list of sleepers; `wake_one` / `wake_all` are IRQ-safe. The list itself is
  scheduler state (IRQs off), not protected by the sleeper's lock.
- `semaphore_t` = `count` + wait queue + `lock`. `down` waits for and consumes one unit; `up` adds
  one unit and wakes one waiter under the lock (non-blocking, IRQ-safe, never loses signals).
- `mutex_t` = `owner` + wait queue + `lock`; not recursive, only the owner may unlock.
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
  is canonical: at most one line per call. EOF follows POSIX: it hands over the pending bytes and is
  always discarded, so a read returns 0 only when EOF starts an empty line (`abc^D` returns `abc`,
  the next read blocks). `console_lock` protects `read_index`/`write_index`; readers sleep with
  `wait_queue_sleep_locked`, the console thread commits and wakes under it. `edit_index` and
  `[write, edit)` belong to the console thread alone (erase and Ctrl+C touch them without the lock).
- **Console self-test** (`selftest/console.c`): a feeder thread injects `h i Enter` and `Ctrl+D`
  through `input_report_key` while the reader is asleep; it must read `"hi\n"`, then 0 (EOF).
- **Output:** `tty_write` decodes UTF-8 (U+FFFD for malformed/overlong) and runs under `irq_save`,
  so each `kprintf` is atomic and callable from IRQs, panic and early boot. The PC `screen_glyph`
  maps code points to CP437 and falls back to the unaccented letter (`ã → a`).
- User programs reach the console through its `file_t` (see Files), never `console_read/write`
  directly.

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
- `console_store` wrapped with `% (CONSOLE_BUFFER_SIZE - 1)`, so the ring diverged after 1023 bytes;
  `console_read` looked for Ctrl+C as EOF.
- `kmalloc`'d `thread_t`/`process_t` left new fields (`process`, `files[]`) as garbage: the reaper
  "released" a random pointer (#GP on a non-canonical address). Both now come from `kzalloc`.
- `process_wait` slept holding `p->lock` (a reaper race panicking "already held", a deadlock on SMP)
  → `wait_queue_sleep_locked`. `file_put` and `process_release` must tear down after the lock, never
  free a held lock. `scoped_guard(...);` with a stray `;` silently guarded an empty body.
- Boot metadata was placed right after the kernel, on top of where GRUB loads modules.
- `elf_load` accepted segments above `USER_TOP`: `USER_TOP - p_vaddr` wrapped, so a segment could map
  into the kernel half. Caught by the ELF self-test's kernel-half case.
- `multiboot_parse_cmdline` left `cmdline` unterminated at 256+ characters; libc `strcpy` never wrote
  the terminator.

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
- **4e (done):** faults with CS RPL 3 kill the thread instead of panicking (NMI, `#DF`, `#MC`
  excluded); `null` and `hlt` self-tests.
- **4f-1 (done):** `process_t` owning the address space, refcount (thread + handle), `process_wait`,
  exit status (128 + signal for faults); tests wait and release instead of sleeping and leaking.
- **4f-2 (done):** `file_t` + `file_ops_t`, console backend on fds 0–2, per-process fd table,
  create/install/start split, fds closed at exit, `SYS_CLOSE`, `files` self-test.
- **Locking refactor (done):** `spinlock_t`, scope guards, `wait_queue_sleep_locked`; files,
  processes, semaphores, mutexes, thread ids, the zombie list and the console on named locks; the
  scheduler documented as IRQs-off on purpose.
- **4f-3 (done):** modules in `boot_info` and kept reserved, separate user build, `elf_load` +
  `exec_load`, `init_start` running `hello` (status 42), ELF self-test with ten rejection cases.
- **4f-4 (in progress):** userland libc.
  - (a, done) uapi split: only `<mihos/...>` reaches the sysroot.
  - (b, done) `libk.a` / `libc.a` from one tree with separate flags.
  - (c, done) `crt0.o`, inline-asm syscall layer, `errno`, `unistd` wrappers, `exit`; `hello` on libc.
  - (d, done) stdio output over fds (`hello` prints with `printf`), `ferror`. Verified once with an
    ordering test run temporarily in `hello` (not kept): screen order `1`, `3`, `2 no newline... 4`,
    `5 flushed by exit` (last, only via `exit`); `fputc`/`fwrite`/`printf`/`fputs("")` return
    values; after `close(2)` `fprintf(stderr, …) < 0` with `ferror(stderr)` and `errno == EBADF`.
    Bring it back as `user/tests/stdio` once `user/` is restructured and init runs test programs.
  - (e, next) `brk` syscall + `malloc`/`free`; more user stack pages.
- **Then (roadmap phase A):** restructure `user/` (`bin/`, `sbin/`, `tests/`, `lib/`; see
  `ROADMAP.md`), stdio input (`fgets`/`getchar`, flushing line-buffered output before reading),
  spawn/exec + wait syscalls with `argv`/`envp`, `/sbin/init` starting a first shell, freestanding
  C++ runtime (`.init_array` in `crt0`, `operator new`, `__cxa_*`).
- **Decisions** (details in `ROADMAP.md`): own syscall ABI and numbering (porting is at the
  POSIX/libc API, not Linux binaries); own libc through phase C, mlibc via sysdeps at phase D
  together with a GCC rebuild for libstdc++; `libk.a` stays ours.

### Console / keyboard layer (done)

PS/2 moved into `platform/pc`, UTF-8 tty with CP437 glyphs and hardware cursor, atomic `kprintf`,
US/ABNT2 keymaps with dead keys, line discipline, blocking `console_read()`.

### Optional — second arch skeleton

A stub `riscv64`/`qemu-virt` (or custom CPU) target implementing the contracts with stubs and a UART
`screen` backend. Link errors reveal remaining x86 assumptions.

## Known technical debt

**Open bugs:** none known.

**Debt:**

- `page_unmap` does not reclaim empty intermediate tables.
- Direct map covers only the first 1 GiB; RAM above it is ignored (clamped in `mmap_init`).
- `MMU_EXEC` ignored: NX not enabled (`EFER.NXE`), every mapping is executable.
- Slab pages are never returned to the buddy allocator (no empty-slab reclamation, no cache destroy).
- `kmalloc` above 1024 B rounds up to a power-of-two number of pages (internal waste).
- `frame_free`/`frame_lookup` walk the zone list linearly.
- No SMP. The spinlock API is ready (only the atomic lock word is missing), but the scheduler, wait
  queue lists, `kmalloc`/`frame_alloc`, `tty_write` and the input ring still rely on IRQs off; SMP
  needs per-CPU run queues and a lock handed over the context switch.
- `input.c`'s event ring still uses `irq_save` (could get an `input_lock` like the console).
- No `in_interrupt()` guard: blocking from an IRQ handler is not detected.
- No `thread_join`; a `thread_t *` from `thread_create` is only valid until that thread exits.
- Sleep queue is scanned linearly on every tick.
- Every block/yield goes through `int`/`iretq`; consider the two-level (Linux/xv6) context switch
  if blocking becomes frequent (hidden behind `arch_yield`).
- `<asm/...>` lives at family level (`arch/x86/include`) but uses x86_64-only instructions.
- Stale headers in the sysroot can hide include errors: do a clean build (`make clean`) after
  moving or removing headers.
- libc has no `signal.h` yet (POSIX's needs `sigaction`); it will wrap `<mihos/signal.h>`.
- `-ffreestanding` is commented out in `common.mk` and there is no `-fno-strict-aliasing`; the kernel
  casts between struct types (`list_container`, slab headers). Check what the MiHOS GCC target
  defaults to and enable both for the kernel.
- No `-Werror`: an incompatible-pointer warning (`spinlock_t *` vs embedded lock) went unnoticed.
- Boot modules stay reserved forever (never returned to the allocator after loading).
- Modules far above the kernel waste the free RAM between them (one boundary, not per-module holes).
- One user stack page (`exec_load`); no stack growth or guard page.
- `NX` still off, so `PF_X`/`MMU_EXEC` from the loader are informational.
- `init_start` looks up a hard-coded module name (`"hello"`).
- `tty_write` holds IRQs off for the whole write plus a full 4000-byte VGA copy; long user writes
  will need a mutex and a separate panic path.
- Screen redraws the whole buffer on every write; scrolling uses `memcpy` on an overlapping range.
- Single tty UTF-8 decoder state; relies on every write going through the IRQ-off section.
- Keyboard LEDs never follow Caps/Num Lock (`0xED` not sent; needs a PS/2 command path).
- No raw mode / termios; Tab, Esc, arrows and Home/End are ignored in line editing.
- Second-port interface test runs even when no second port was detected (may hang on
  single-channel controllers).
- `vsnprintf`: a precision is parsed and ignored (`%.3s` prints the whole string); unknown
  specifiers are silently dropped; `%s` does not bound-check `remain`; it returns the characters
  written, not the full length C99 requires, so `printf` silently truncates at `BUFSIZ - 1`.
- stdio: no input side yet (`fgets`, `getchar`, `__fillbuf`), no `fopen`/`fclose`/`setvbuf`, no
  `isatty` (stdout is always line-buffered), `__stdio_head` is `const` (must change when `fopen`
  adds streams), no stream locking (needed with user threads); `stdio.h` is still partly a stub
  (`fopen`, `fread`, `fseek` declared but missing).
- Bochs (win32 GUI) sends Left Ctrl for the ABNT2 `/ ?` key, so `KEY_RO` can only be tested on QEMU.
- No IDT gate uses the IST stacks yet; a double fault on a broken kernel stack still triple-faults.
- `mmap_split_region` drops region remainders smaller than 16 frames.
- `address_space_map` leaves already-mapped pages in place when it fails midway (freed on destroy).
- Syscalls run with IRQs off for their whole duration (interrupt gate); long writes delay IRQs.
- The fd table has no lock (safe only with one thread per process and no `fork`).
- No self-test proves memory comes back: compare free frames (and the console file's refs) before
  and after `user_selftest` to catch a missing `process_release` / `file_put`.
- Fault messages print the thread id, the exit line prints the pid.
- `process_wait(NULL)` returns -1, which is also a valid exit status (should assert).
- `process_start` returns -1 instead of an errno on failure.
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

Inspect a user program's segments (each `LOAD` page-aligned, no overlaps):

```sh
x86_64-mihos-readelf -l sysroot/usr/bin/hello.elf
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
