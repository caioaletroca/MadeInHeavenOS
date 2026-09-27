# MadeInHeavenOS Architecture

Modular monolithic x86-64 kernel. One statically-linked kernel image, no userspace yet.

```text
Docker x86_64-mihos toolchain
  -> libc/libk.a -> kernel ELF -> sysroot -> isodir -> mihos.iso
  -> QEMU / Bochs -> GRUB Multiboot2 -> _start -> boot64 -> kmain
```

Composition root: `kernel/kmain.c:11-33`

```text
kmain(phys)
  interrupts_init()  # kernel/cpu/interrupts.c:3-15
  ps2_init()         # kernel/driver/ps2.c:3-44
  sti                # kernel/kmain.c:20
  mmap_init(info)    # kernel/mm/mmap.c:136-193
  paging_init()      # kernel/mm/paging.c:112-119
  slab_init()        # kernel/mm/slab.c:256-270
  hlt loop
```

## Subsystems

| Area | Location | Role |
|------|----------|------|
| Boot/arch | `kernel/arch/x86_64/` | Multiboot header, 32->64 transition, GDT/TSS, IDT/ISR, PIC, port I/O, VGA, linker |
| CPU policy | `kernel/cpu/` | `interrupts_init`, `exception_init`, panic-on-fault |
| Drivers | `kernel/driver/` | screen backbuffer, TTY, PS/2 policy, keyboard IRQ33 |
| MM | `kernel/mm/` + `kernel/include/mm/` | mmap -> zone -> buddy -> frame -> slab; paging scaffold |
| Diagnostics | `kernel/kprintf.c`, `kernel/sys/panic.c` | `kprintf`, fatal `panic` |
| Runtime | `libc/` | stdio/string/stdlib, `sys/list.h`, `sys/hashtable.h` |
| Packaging | `grub/` | `grub.cfg`, `grub-mkrescue` -> `mihos.iso` |
| Toolchain | `cross-compiler/`, `generic-cross-compiler/` | Docker cross environments |

Build units: `Makefile:1-4`, `common.mk:1-36`, `libc/Makefile:1-24`, `kernel/Makefile:1-50`, `grub/Makefile:1-18`.

## Key boundaries

Good seams:

- `driver/screen.c` generic buffer vs `arch/x86_64/screen.c` VGA flush
- `driver/ps2.c` policy vs `arch/x86_64/ps2.c` port I/O
- ISR registry: `arch/x86_64/isr.c:3-16` + `include/isr.h:40-46`
- MM layers: `mmap.c` -> `frame.c` -> `zone.c` -> `buddy.c` -> `slab.c`

Leaky / missing:

- No syscall, scheduler, VFS, process, user split
- `libk.a == libc.a` content, see `LIBC_LIBK.md`
- `paging_init` only installs vector 14 handler
- See `KNOWN_BUGS.md`

## Read next

- `ADDRESS_SPACE.md` — higher-half, linker, boot map
- `INTERRUPTS.md` — IDT/ISR/PIC/PS2/keyboard
- `MEMORY.md` — allocator stack
- `CONSOLE.md` — kprintf path
- `LIBC_LIBK.md` — runtime boundary
- `BUILD_RUN.md` — Docker, sysroot, QEMU/Bochs
- `KNOWN_BUGS.md` — backlog with file:line
