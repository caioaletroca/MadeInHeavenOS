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

**The kernel stays POSIX-shaped**: small integer fds, `fork`/`exec`, signals, one file namespace,
`mmap`. That keeps the libc layer thin. It does not mean copying Linux's quirks.

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
| syscalls run with IRQs off | B (disk I/O blocks for long) |
| 1 GiB direct-map limit | B / D (caches, large programs) |
| NX off | D (`mprotect`, JITs) |
| single CPU scheduler design | D (SMP) |
| VGA text console | F |

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
3. Restructure `user/` as above; `hello` → `user/tests/hello`, the `user_program.S` tests and the
   temporary stdio/brk/malloc tests → libc programs in `user/tests/`.
4. stdio input (`fgets`, `getchar`; flush line-buffered output before reading).
5. Spawn/exec + wait syscalls, `argv`/`envp` on the initial stack; `init` starts a first `sh` with
   builtins.
6. Phase B: initramfs + VFS; the shell runs programs by path.

## Reference projects

SerenityOS (C++, own everything, `Ports/`), Sortix (own POSIX libc, self-hosting), managarm + mlibc
(POSIX on a microkernel, `xbstrap`), ToaruOS (own compositor and GUI), Lyre / Astral / Ironclad
(mlibc users), Redox (Rust, relibc), Haiku (native API + POSIX).
