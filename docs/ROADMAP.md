# Roadmap — MadeInHeavenOS

Where the project is going, and the decisions that keep the small steps pointed there. The session
handoff (`SESSION_HANDOFF.md`) says where we are; this file says where we are going and why.

## End goal

A self-hosting, POSIX-shaped desktop OS: own kernel, own system and GUI, third-party software
rebuilt for it. Concretely, in rough order:

- a shell and the usual command-line tools
- a real filesystem on disk (ext2, then ext4)
- compiling C/C++ (and eventually the OS itself) inside MiHOS
- networking
- a framebuffer GUI with windows, written in C++
- modern runtimes: QuickJS first, Node.js as the final boss
- installation onto a hard drive, real hardware

## Where the line is

```text
 applications (vim, gcc, quickjs, MiHOS GUI apps)
 ───────────────────────────────────────────── ③ native API: MiHOS toolkit, services, IPC (C++)
 libraries (freetype, SDL, libgui)
 ───────────────────────────────────────────── ② POSIX / ISO C API   ← third-party software connects here
 libc
 ───────────────────────────────────────────── ① syscall ABI         (our own, not Linux's)
 kernel
```

**Line ② with a bit of line ③.** Software is ported by recompiling it against MiHOS's POSIX API, not
by running Linux binaries. The syscall ABI is ours, since only libc ever sees it. The native API (line
③) is where MiHOS has its own identity: services, window server, GUI toolkit.

**The rule:** anything that defines how MiHOS works is ours; anything that just needs to run on it
is borrowed.

| Ours | Borrowed (ported through line ②) |
|---|---|
| kernel: scheduler, memory, VFS, drivers, syscall design | libc (from phase D: mlibc) |
| filesystem drivers (ext2/ext4 is a format, the driver is ours) | GCC, binutils, make |
| `init`, system services | busybox/coreutils, editors |
| shell (ours, and/or a port of dash) | Lua, Python, QuickJS → Node |
| window server, GUI toolkit, native apps (C++) | FreeType, zlib, libpng, SDL, later Mesa |
| installer, boot | GRUB (until an own bootloader) |
| PCI enumeration, disk/USB/network drivers | ACPI tables and AML (uACPI or ACPICA) |
| | a USB host stack, if writing our own isn't worth it (CherryUSB to evaluate first) |

**The kernel stays POSIX-shaped**: small integer fds, `fork`/`exec`, signals, one file namespace,
`mmap`. That keeps the libc layer thin. It does not mean copying Linux's quirks.

## Kernel shape

**A modular monolith, with user-mode services where they pay off** (decided after phase A,
revisiting the microkernel question once user space existed).

- **In the kernel:** what many processes share, what needs IRQs or DMA, what is needed to boot:
  the scheduler, memory, VFS, file systems, disk/USB/network drivers. Drivers stay behind narrow
  interfaces (`block_device_ops`, the VFS node ops), the same shape a driver protocol would have.
- **In user mode:** line ③'s services (window server, input, audio), and policy rather than
  mechanism (device naming, configuration), the way Linux leaves printers, scanners, Mesa and
  FUSE to user space.
- **A user-mode driver experiment** once phase C has IPC: a small device (serial port or
  keyboard) driven from ring 3. The kernel hands over the device's I/O ports (the TSS I/O
  permission bitmap: `in`/`out` work in ring 3, no syscall), maps its MMIO registers into the
  driver, and delivers its IRQ as a message; only that hand-over and the IRQ wake-up cost a
  syscall. Microkernel mechanics, learned without betting POSIX on them.
- **Why not a microkernel:** POSIX (`fork`, signals, file `mmap`, shared fds) spans several
  servers there, which managarm and Redox spend years on and Fuchsia gave up on; and phase B
  (ATA, partitions, cache, ext2, VFS, devfs) would be rewritten as servers. The cost of a
  microkernel is the IPC hops between components (app → VFS → ext2 → driver), not the drivers
  touching hardware.

## The libc plan

1. **Phases A–C: our own libc.** The point is to learn how stdio, `malloc` and process startup work.
   It grows with each phase (stdio and `malloc` now, files in B, the process model in C).
   **Graduation test:** Lua (pure ISO C) builds and runs, then QuickJS.
2. **Start of phase D: switch to mlibc**, a portable libc made for hobby OSes. We write its *sysdeps*
   (`sys_open`, `sys_read`, `sys_vm_map`, … on top of our syscalls; our numbering stays). This is
   also a **toolchain milestone**: GCC's `x86_64-mihos` target is rebuilt against mlibc and gains
   **libstdc++**.
3. **Triggers for the switch**, whichever comes first: needing libstdc++; self-hosting GCC; ports
   failing on whole categories (locales, `wchar_t`, `regex`, `iconv`, full `printf`); user-space
   pthreads.
4. **What stays ours:** `libk.a` (the kernel never uses mlibc), the sysdeps layer, and everything
   learned. Never run two libcs in user space at once: the switch is a clean cut.

**Keep the switch cheap, starting now:** thin syscall wrappers (logic in the kernel); POSIX-correct
public headers, types and signatures (`off_t` 64-bit, `mode_t`, `pid_t`, `ssize_t`); `FILE` opaque;
user programs never use `__` internals; the uapi headers stay the documented kernel ABI.

**C++ before the switch:** freestanding C++ (`-fno-exceptions -fno-rtti`, own containers, `operator
new` on our `malloc`, `.init_array` in `crt0`), the way SerenityOS's `AK` started. Wanting `std::`
badly is a reason to move the switch earlier.

## Phases

| Phase | Capability | Main pieces | Unlocks |
|---|---|---|---|
| **A** | Programs that do things | libc stdio + `malloc`/`brk`, more stack pages, spawn/exec + wait, `argv`/`envp` | `init` and a first shell with builtins |
| **B** | Files | VFS (inodes, path lookup, mounts), **initramfs** (tar from GRUB), `/dev`, block driver (AHCI or virtio-blk), GPT, **ext2 → ext4** | programs by path, persistent storage |
| **C** | POSIX process model | `fork` (copy-on-write), `execve`, `waitpid`, signals, pipes, `dup2`, `chdir`, TTY/termios (raw mode), process groups | real shells (dash), busybox, editors; **Lua** as the libc test |
| **D** | Porting platform | **mlibc + toolchain rebuild (libstdc++)**, `mmap`/`mprotect`, pthreads + futex, TLS, clocks, SMP | **self-hosting** GCC/binutils/make, Python, QuickJS |
| **E** | Network | NIC driver (virtio-net/e1000), TCP/IP (own or lwIP), sockets, `poll` | networked software; with D, Node.js |
| **F** | Graphics | framebuffer (Multiboot2 tag), mouse, user-space compositor/window server (C++), FreeType, GUI toolkit | windows, GUI apps, an IDE inside MiHOS |
| **G** | Real installation | installer, bootloader on disk, UEFI | MiHOS on real hardware |

Sizing: A–C is the classic hobby-OS arc. D (self-hosting) is the big milestone. Node.js needs D and E
together (C++17 libstdc++, pthreads, executable `mmap` for V8's JIT, signals, sockets, libuv).

### Debt that blocks later phases

| Debt | Needed by |
|---|---|
| fixed 64 KiB user stack, no growth | C |
| syscalls run with IRQs off | D (latency; disk waits already sleep, since IRQ-driven ATA) |
| 1 GiB direct-map limit | B / D (caches, large programs) |
| NX off | D (`mprotect`, JITs) |
| single CPU scheduler design | D (SMP) |
| VGA text console | F |

## Hardware targets

**Rule of thumb: the hardware in the development PC** (an Intel 600/700-series chipset). Older
hardware only where QEMU makes it free (the IDE/ATA driver).

| Device | How the OS reaches it | Order |
|---|---|---|
| PCI(e) enumeration | config space: ports `0xCF8`/`0xCFC`, then ECAM (address from the ACPI `MCFG` table); vendor/device/class IDs, BARs, bus mastering, MSI/MSI-X | **first**: every modern device below is found through it |
| NVMe SSD (the PC's disk: PCIe ×4 in an M.2 slot) | MMIO registers, submission/completion queues in RAM, a doorbell write per batch | 2 (or AHCI first) |
| SATA disks | AHCI (MMIO) | 2 |
| USB | **one xHCI controller** covers every port and speed (UHCI/OHCI/EHCI are absent on modern PCs; USB-C is a connector). Then enumeration, hubs, HID and mass storage classes | 3; until then the firmware's PS/2 emulation covers USB keyboards |
| Wired network | per chip (Intel/Realtek) | phase E |
| GPU | **the UEFI/Multiboot2 framebuffer** (a pixel buffer, no GPU driver); acceleration much later, Intel first (documented), NVIDIA not realistic | phase F; to be discussed then |
| Wi-Fi | per chip, binary firmware, 802.11 | not planned |

A custom CPU (bare metal) has none of these: its own simple devices (UART, timer, interrupt
controller), plus **virtio** devices if a QEMU model exists (virtio does not depend on the
architecture). That target relies on the arch contract staying narrow.

## Repository layout target

```text
kernel/                   # the kernel (uapi headers in kernel/include/uapi/mihos/)
libc/                     # libk.a (kernel) + libc.a + crt0.o; replaced by mlibc in user space at D
user/
├── user.ld
├── lib/                  # own user-space libraries (C++ later): libcore, libipc, libgui
├── sbin/init/            # PID 1
├── bin/                  # own utilities: sh, echo, cat, ls …
├── services/             # daemons: window server, …
├── apps/                 # GUI applications (phase F)
└── tests/                # user-mode test programs, run and checked by exit status
ports/                    # third-party software: build scripts + patches, never vendored sources
grub/                     # bootloader config and ISO
docs/                     # handoff, roadmap
```

- The directory decides the install path: `user/bin/echo` → `/bin/echo`, `user/sbin/init` →
  `/sbin/init`. From phase B the initramfs is built from the sysroot, so build tree, sysroot and the
  running system match, and programs lose the `.elf` suffix.
- `ports/<name>/` downloads a pinned upstream version, applies MiHOS patches, builds with the cross
  toolchain into the sysroot (like SerenityOS `Ports/` or managarm's `xbstrap`).
- `user/Makefile` discovers program directories under `bin/`, `sbin/`, `tests/`, … instead of a fixed
  list, and installs by category.

## Near-term order

1. ~~Finish and commit stdio (fix `fwrite`/`fputs` return values, run the ordering test).~~ Done.
2. ~~`brk` syscall + `malloc`/`free` in libc; more user stack pages.~~ Done.
3. ~~Restructure `user/` as above; `hello` → `user/tests/hello`, the temporary stdio/brk/malloc
   tests → libc programs in `user/tests/`.~~ Done; the `user_program.S` kernel self-tests too
   (all but the ring-3 loop), after W6.
4. ~~stdio input (`fgets`, `getchar`; flush line-buffered output before reading).~~ Done.
5. ~~Spawn + wait syscalls, `argv`/`envp` on the initial stack~~ (done; `exec` comes with `fork` in
   phase C); ~~`/sbin/init` in user space~~ (done: pid 1, runs the test runner) ~~starts a first
   `sh` with builtins~~ (done: `/bin/sh` with `exit`/`help`/`history` and `PATH` search; init
   restarts it with `respawn` and halts through `reboot` with `once`). Shell extras done: quotes, `$?`/`$NAME`, `!!`/`!n`.
6. ~~Freestanding C++ runtime~~ (done: constructors and destructors, `atexit`, `operator new`).
   **Phase A complete.**
7. Phase B: ~~initramfs~~ (done: one ustar `initrd.tar`, programs without `.elf`) + VFS (step (a)
   done: nodes, path lookup, the initrd mounted as `/`; step (b) done: `open`/`read`/`lseek`;
   step (c) done: `getdents`, libc `opendir`/`readdir`, `ls`, a test runner that lists its tests;
   step (d) done: `stat`/`fstat`; `/dev` done: mounts, devfs, `/dev/null`, `/dev/zero`,
   `/dev/console`, `ioctl`; disks in progress: the block layer, an ATA PIO driver, block
   devices in `/dev`, the block cache and read-only ext2 on `/mnt` done; writing in progress
   (W1, sector writes and flushes, W2, write-back in the cache, W3, a writable `/dev/hda`,
   and W4, ext2 overwriting file data, done; ext2 allocation: inode write-back, the block
   allocator, growing files and `s_state` done, checked by e2fsck; namespace: truncate, the
   inode allocator, creating and deleting files, `O_APPEND`, `mkdir`/`rmdir` and a full-disk
   test done: **ext2 writing is complete**; the test suite reviewed after it; IRQ-driven ATA
   and MBR partitions done), then the root on the disk: `root=` on the kernel command line,
   a pre-installed ext2 on a partition, the initrd as the live system; the installer is phase G).

## Reference projects

SerenityOS (C++, own everything, `Ports/`), Sortix (own POSIX libc, self-hosting), managarm + mlibc
(POSIX on a microkernel, `xbstrap`), ToaruOS (own compositor and GUI), Lyre / Astral / Ironclad
(mlibc users), Redox (Rust, relibc), Haiku (native API + POSIX).
