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
- User-mode threads preempted by the timer; `int 0x80` syscalls `write`, `read`, `close`, `exit`,
  `brk`, `spawn` and `wait` with user pointer validation
- Programs start with `argc`/`argv`/`envp` on a System V initial stack; parents spawn children
  (programs found by path in the initramfs) and wait for one or any of them with a POSIX
  wait status
- User heap after the ELF image (`brk`), 64 KiB user stack with a guard page below it
- Processes (`process_t`) owning their address space and fd table, waitable, refcounted; faults in
  ring 3 kill the process with 128 + signal instead of panicking
- Open files (`file_t` + `file_ops_t`), `/dev/console` behind fds 0–2
- **VFS**: `vfs_node_t` per file or directory, path lookup (`.`, `..`, repeated `/`), the
  initrd (one ustar archive GRUB loads as the `initrd` module) mounted as `/` as a tree of nodes;
  `open`/`lseek` on files, `getdents` on directories, `stat`/`fstat`; mounts, with devfs
  mounted at `/dev` with `/dev/null`, `/dev/zero` and `/dev/console`; `ioctl` (`TIOCGWINSZ`)
- **ELF64 loader**: user programs built separately (`user/`), validated and mapped by `elf_load`
- **`/sbin/init` as pid 1**: the only program the kernel starts (the kernel command line is its
  arguments); `once <command>` (the test entries) runs it and halts the system through `reboot`,
  `respawn <command>` (the shell entry) restarts it forever. Init never exits (the kernel panics)
- **User test programs** in `user/tests/` (`hello`, `stdio`, `brk`, `malloc`, `malloc_abort`,
  `stdin`, `args`, `spawn`, `printf`, `reboot`, `cxx`, `files`, `dirs`, `stat`, `tty`, `devices`, `ioctl`, `disks`, `ext2`; `stdin_interactive` from a second GRUB entry), run by `/usr/tests/run`
- **A shell** (`/bin/sh`, third GRUB entry): builtins `exit`/`help`/`history`, a ring-buffer
  history with `!!`/`!n`/`!-n`, quotes and `\` escapes, `$?`/`$$`/`$NAME`, `PATH` search;
  `/bin/echo` as the first command; `/bin/ls` (`-a`, several paths, `/` after directories)
- **uapi headers** (`<mihos/syscall.h>`, `<mihos/errno.h>`, `<mihos/signal.h>`, `<mihos/fcntl.h>`,
  `<mihos/dirent.h>`, `<mihos/stat.h>`): the only kernel
  headers installed into the sysroot
- **Own libc**: `libk.a` (kernel) and `libc.a` + `crt0.o` (user) from one tree; inline-asm syscall
  layer, `errno`, `write`/`read`/`close`/`_exit`/`brk`/`sbrk`, `exit`/`abort`; buffered stdio over
  fds with `printf`, `fgetc`/`fgets`/`ungetc`; `malloc`/`free`/`calloc`/`realloc` on `sbrk`;
  `atexit`; `open` (with a mode)/`creat`/`lseek`/`unlink`, `mkdir`/`rmdir`, `opendir`/`readdir`/`rewinddir`/`closedir`; a freestanding C++
  runtime (global constructors and destructors, `operator new`)
- **Disks:** a block layer (`block_read`/`block_write` with range checks, `block_flush`) and an
  ATA PIO driver (polling, 28-bit LBA, reads, writes and cache flushes) finding `hda`…`hdd`; an ext2 test disk (`disk.img`) attached in the emulators;
  each disk as a block device in `/dev` (`/dev/hda`, `S_IFBLK`, seekable, read and written at
  byte offsets through `block_read_bytes`/`block_write_bytes`); a **block cache** (`block_cache_get`/`block_cache_put`: 64
  leased 4 KiB buffers, hash lookup, LRU eviction, write-back of dirty buffers on eviction and
  `block_cache_sync`, run at halt) under every disk read
- **ext2:** the first ext2 disk mounted on `/mnt` at boot; directories, file reads
  through direct and indirect blocks (holes read as zeros), real modes, times and link counts;
  one cached node per inode, freed after the last reference; files written anywhere: holes
  filled and files grown through a block allocator (bitmaps, free counts, new indirect
  tables), truncated, and created (`O_CREAT`: an inode allocator, names added to
  directories), deleted (`unlink`, freed at the last close), appended to (`O_APPEND`),
  directories made and removed (`mkdir`/`rmdir`), checked by `e2fsck` after every `./run-tests.sh`
- Intrusive `list.h` and an hlist `hashtable.h` (multiplicative `hash_64`) shared by kernel and libc
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
- Guard, hash table, MM, preemption, sync, console, user-mode, VFS, block, ext2 and ELF self-tests and the user test programs
  passing in Bochs (stdin_interactive by hand); keyboard verified in Bochs and QEMU

Latest commits:

```text
c708ead test: make e2fsck part of every run-tests.sh run
cd60060 test: drop ext2's inode write-back test and ext2_inode_write
7d8dc17 test: drop the raw user-mode programs the libc tests cover
d0c8ac7 test: move the kernel-address EFAULT and hlt checks into user tests
cbd70d3 docs(architecture): document the full-disk test and W6's end in handoff
30d8da8 test: fill the disk
abc1c1c fix(fs): clear i_links_count when ext2 frees an inode
e8fad0d docs(architecture): document mkdir and rmdir in handoff
b53f81d test: cover mkdir and rmdir
21248bc feat(libc): add mkdir and rmdir
3073dfb feat(sys): add mkdir and rmdir
f3f0f77 feat(fs): mkdir and rmdir on ext2
9fe3ebd feat(fs): add mkdir and rmdir to the VFS
64d03d9 docs(architecture): document the fs/ext2/ layout in handoff
a10f718 refactor(fs): move what is left of ext2.c, mounting, into mount.c
fefae0f refactor(fs): split ext2's create, unlink and the release into namespace.c
e0ad82a refactor(fs): split ext2's file reads, writes and truncates into file_data.c
485bb19 refactor(fs): split ext2's directories into directory.c
ec5cff5 refactor(fs): split ext2's file block mapping into file_blocks.c
e9e9b36 refactor(fs): split ext2's inodes, nodes and the ops tables into inode.c
7343c9b refactor(fs): split ext2's block and inode allocation into allocation.c
1077e3d refactor(fs): split ext2's disk access into disk.c
418999d refactor(fs): move ext2 into fs/ext2/
38e0ee8 docs(architecture): document unlink and O_APPEND in handoff
7e9114f test: cover unlink and O_APPEND
98a12b0 feat(fs): add O_APPEND
2ccd035 feat(libc): add unlink
6f09911 feat(sys): add unlink
2136b65 feat(fs): unlink ext2 files
ec624fc feat(fs): add unlink to the VFS
8312956 docs(architecture): document the inode allocator and creating files in handoff
812e84c test: cover creating files
301e8c9 feat(libc): read open's mode and add creat
20896e9 feat(fs): create ext2 files
d4b8431 feat(fs): add create to the VFS and O_CREAT/O_EXCL
1ceb91f docs(architecture): document truncate in handoff
be350f9 test: cover truncate
d516063 feat(libc): add ftruncate and truncate
484ec54 feat(sys): add ftruncate and truncate
bc6e544 feat(fs): truncate ext2 files
8fb1dd9 feat(fs): add truncate to the VFS and O_TRUNC
856b1c7 docs(architecture): document halt hooks and ext2's s_state in handoff
35df6cc test(fs): cover ext2's s_state
782c837 feat(fs): mark ext2 volumes not clean while mounted writable
cf8373c feat(sys): add halt hooks
56e5bf3 docs(architecture): document ext2 allocation and fsck-test.sh in handoff
230fc2a build: make fsck-test.sh executable
5ba9c1c test: run e2fsck on a disk the tests wrote
4496112 test(fs): cover ext2 allocation
6bf5dba feat(fs): grow ext2 files
743813a build: add files for the tests to grow to the test disk
2bf9e6d feat(libc): add EFBIG
0284433 feat(fs): allocate and free ext2 blocks
18913d0 feat(fs): check ext2 groups fit their bitmaps
8412aa3 refactor(fs): order ext2.c bottom-up, in sections
04b28bd docs(architecture): document ext2 inode write-back in handoff
2778f7d test(fs): cover ext2 inode write-back
38d3cf4 feat(fs): write ext2 inodes back
998eef0 refactor(fs): find ext2 inodes with ext2_inode_byte
f1a5f12 docs(architecture): document ext2 overwrites in handoff
d6b1056 test(fs): cover ext2 overwrites
8d74a1f feat(fs): overwrite ext2 file data
98a7a77 feat(fs): give ext2 nodes a lock
1f35c29 docs(architecture): document writable block devices in handoff
5c2e489 test: cover writing /dev/<disk>
5ce0df4 fix(sys): count the bytes write returned
7a8b286 feat(driver): write block devices through /dev
b7579b6 feat(fs): write files through the VFS
376354c feat(driver): add block_write_bytes
cce9373 feat(libc): add ENOSPC
ac37841 docs(architecture): document the cache's write-back in handoff
35bde4a test(block): cover dirty buffers, write-back and sync
299dc4c feat(sys): sync the block cache at halt
a8c63af feat(driver): add block_cache_sync
63996e2 feat(driver): write dirty cache buffers back on eviction
7862149 refactor(driver): rename the cache's fill_lock to data_lock
c378688 docs(architecture): document ATA writes and the writing plan in handoff
a6f8068 test(block): cover writes and flushes
f5dd637 feat(platform): write and flush ATA disks
568f6e1 refactor(platform): share the ATA command setup
53bf336 feat(driver): add block_write and block_flush
46c823b docs(architecture): add naming rules and the new names to handoff
14ba902 refactor(fs): rename ext2's internals plainly
91ea15a refactor(fs): name mount fields and node links plainly
0635a36 refactor(driver): rename bcache to block_cache
f7734af docs(architecture): document ext2 in handoff
5060b51 test: cover ext2
beb4b3b feat(fs): mount the first ext2 disk at /mnt
9d8ebf1 feat(fs): read ext2 files
1928bba feat(fs): list and look up ext2 directories
8219ac0 feat(fs): add ext2 inodes and the node cache
e08c80d feat(fs): read the ext2 superblock and group descriptors
29a49d4 build: keep the test disk's files byte-exact in git
fb49d44 build: add /mnt and a richer ext2 test disk
89c64df refactor(libc): name list and hlist operations consistently
e9d610b test: cover the hash table and the block cache
ec8c250 refactor(driver): read block devices through the cache
878b25a feat(driver): add the block cache
d279f2f fix(sched): declare mutex_lock before the mutex guard uses it
d9afb83 feat(libc): add list_move_after
0ba5040 feat(libc): replace the hash table with hlist and a multiplicative hash
d452e51 docs(architecture): document block device nodes in handoff
3bce5df test: cover block devices
beb1f9d feat(libc): add S_IFBLK, S_ISBLK and DT_BLK
0ce32df feat(driver): register block devices in devfs
efb5943 feat(driver): read block devices at byte offsets
ff81743 feat(fs): add block device nodes
97fb16c docs(architecture): document the block layer and the ATA driver in handoff
c66eb53 test(block): add a block self-test on the test disk
996ab12 feat(platform): add an ATA PIO disk driver
9acb8ee feat(driver): add the block layer
383d442 build: add an ext2 test disk
839d667 docs(architecture): document ioctl and the end of /dev in handoff
0e16d70 test: cover ioctl and the terminal check in isatty
4237875 feat(libc): add ioctl and ask the terminal in isatty
abd2fc3 feat(driver): answer TIOCGWINSZ on the console
2911485 feat(sys): add ioctl
80d890b docs(architecture): document /dev/console in handoff
ebca23e test(user): cover /dev/console
6646fd1 refactor(fs): drop fstat's node-less special case
37bcb4c feat(driver): open the console through /dev/console
67c3e1c docs(architecture): document character devices, null and zero in handoff
6f880fe test: cover character devices
e969784 feat(libc): add DT_CHR and sys/sysmacros.h
bff3748 feat(driver): add /dev/null and /dev/zero
7cc9d4e feat(fs): register devices in devfs
a190f82 feat(fs): add character device nodes
360cc70 refactor(fs): put a file's node in file_put
46f8010 docs(architecture): document mounts and devfs in handoff
ec25f8b test: cover mounts and devfs
6e91058 feat(fs): add devfs, mounted at /dev
e56c63c build(grub): add an empty dev/ to the initrd as a mount point
7e440ce feat(fs): mount file systems on directories
c8e3fc9 docs(architecture): document isatty and stdout's buffering in handoff
59f8345 test(user): cover isatty and stdout's buffering decision
dcba28d feat(ls): list files given as arguments
95b7fee feat(libc): line-buffer stdout only on a terminal
053b6c3 feat(libc): add isatty
7445614 docs(architecture): document stat and fstat in handoff
2054c10 test(user): cover stat and fstat
819610f feat(libc): add stat and fstat
e68b9cf test(fs): cover stat in the VFS self-test
2eb1660 feat(fs): add stat and fstat
7db0e29 refactor(sys): copy user paths in one helper
c1e1b10 docs(architecture): document the self-listing test runner in handoff
d9d7384 refactor(test): find the tests with readdir
8e96296 docs(architecture): document ls and strerror in handoff
ef5edda feat(ls): add ls
637130b feat(libc): define EXIT_SUCCESS and EXIT_FAILURE in stdlib.h
65b1c4a feat(libc): add strerror
8346705 docs(architecture): document readdir, getdents and dirent in handoff
4dc2f6d test(user): cover opendir, readdir and getdents
5c9bb1a feat(libc): add opendir, readdir, rewinddir, closedir and getdents
11f981e test(fs): cover readdir and getdents in the VFS self-test
cb2437e feat(fs): list directories with getdents and O_DIRECTORY
d784a00 docs(architecture): document open, lseek and file positions in handoff
eb29388 test(user): cover open, read, lseek and close on files
dba75da feat(libc): add open and lseek
9f7697b feat(fs): open files through the VFS and seek in them
a1945bd feat(sched): add a mutex guard
f14129e docs(architecture): document the VFS core and the initrd tree in handoff
e0f666d test(user): spawn of a directory, a file with '/' and a relative path
654ab46 feat(fs): mount the initrd as a VFS tree and find programs by path
f899388 feat(fs): add the VFS core
6c28da3 docs(architecture): document the initramfs in handoff
2dbb6a3 feat(exec): load programs from an initramfs
b4e3e2a feat(fs): add an initrd reader for ustar archives
ac14f64 feat(libc): add strnlen
68d6605 docs(libc): document atexit in stdlib.h
1de9855 docs(architecture): document the C++ runtime and the end of phase A in handoff
d2df97b test(user): add a C++ runtime test
5447b68 feat(libc): add the freestanding C++ runtime
cc2901a feat(libc): run global constructors and exit handlers
87d9b30 fix(libc): make stdbool.h and unistd.h usable from C++
7beaa43 build: compile C++ for user space and collect constructor arrays
3deb87f refactor(mm): keep mmap's private helpers out of its header
a5d8aa7 docs(architecture): document the shell tokenizer and expansions in handoff
a85b547 feat(sh): expand !!, !n and !-n from the history
089736d refactor(sh): check a variable name's first character once
df44eb9 feat(sh): split commands with a quote-aware tokenizer and expand $
a02dc21 test: add run-tests.sh, a headless QEMU test run
f2b6c0c docs(architecture): document power off, the debug console and getpid in handoff
554cb4f test(user): cover getpid, getppid and adoption by init
1fbb87a feat(sys): add getpid and getppid
e73d622 feat(platform): mirror console output to the 0xE9 debug port
418c311 feat(platform): power off Bochs and QEMU on halt
5109905 fix(arch): use the operand's width in the port I/O helpers
330c95d docs(architecture): document orphans going to init in handoff
12789bb feat(sched): hand orphans to init
9cb20ad build(grub): show the boot menu for 5 seconds
1f04740 docs(architecture): document reboot and init's once and respawn modes in handoff
ae1ee7b test(user): add the reboot permission test
cc2bae7 feat(init): add once and respawn modes; init never exits
509a9a8 feat(libc): add reboot
2b13247 feat(sys): add SYS_REBOOT, allowed only for init
6cbe22d docs(architecture): document wait for any child and the quiet tests in handoff
1d20f15 test(user): cover wait and waitpid(-1) in the spawn test
d42b444 refactor(test): print only failures and one line per passing test
965ce0f feat(libc): add wait
1bc19f3 feat(sys): wait for any child with pid -1
a2a78d1 docs(architecture): document the printf test and the hh/h fix in handoff
937a1e8 test(user): add a printf test for vsnprintf
5a71556 fix(libc): narrow hh and h arguments in vsnprintf
7a8759b docs(architecture): document the shell, the new vsnprintf and the ISR fix in handoff
6d65c61 feat(sh): add a shell with builtins, history and PATH search, and echo
12e3d59 feat(libc): rewrite vsnprintf with C99 semantics and add snprintf
3f99584 feat(libc): add strspn, strcspn, strpbrk, strchr, strncmp, strtok, strdup, getenv
d05aa8a fix(arch): align the stack for C in interrupts and keep SSE out of the kernel
ed27d4a docs(architecture): document /sbin/init and the user-space test runner in handoff
ef89e4a feat(init): start /sbin/init as pid 1 and run the tests from user space
b9cb7ce docs(architecture): document the initial stack, spawn and wait in handoff
66134b6 feat(sys): add spawn and wait
122c426 feat(exec): pass argc, argv and envp on the initial stack
771202b docs(architecture): document stdio input and the stdin tests in handoff
8201ee2 test(user): add stdin tests, interactive ones in their own GRUB entry
a40d4c9 feat(libc): add stdio input (fgetc, fgets, ungetc, feof, clearerr)
d2c534e docs(architecture): document the user/ layout and the test runner in handoff
0884fb3 test(user): turn the stdio, brk and malloc checks into test programs
8f65e7c refactor(user): group programs by category and run them all as tests
b386b3d docs(architecture): document brk, the user heap and malloc in handoff
f429a19 feat(libc): add malloc, free, calloc and realloc
87bde28 feat(libc): add brk and sbrk
0fbb27a feat(sys): add the brk syscall with the heap after the ELF image
5cea546 feat(exec): map a 64 KiB user stack
82ae59a feat(mm): add address_space_unmap and roll back failed maps
b1f3b43 docs(architecture): record the stdio ordering test in handoff
0031357 feat(libc): add ferror
f7e1942 feat(libc): add buffered stdio over file descriptors
02e12fd feat(libc): build libc.a for user programs with syscall wrappers
1d3b986 feat(libc): add strlcpy and strcmp, terminate strcpy
cd7b843 refactor(driver): protect the console ring with a spinlock
8f5f1d8 refactor(sched): put semaphores, mutexes and thread lists on spinlocks
```

## Architecture

### Layers

```text
libc/                             # libk.a (kernel) + libc.a + crt0.o (user); see "Userland libc"
user/                             # user.ld, include/ (test.h), bin/ sbin/ tests/<program>/
grub/                             # grub.cfg + ISO build (kernel + initrd.tar)
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
├── platform/pc/                  # pic, pit, ps2, keyboard, vga, screen (CP437), multiboot2, ata, platform.c
├── sched/ mm/ driver/ sys/ fs/ exec/ selftest/
├── init.c                        # init_start: starts /sbin/init (pid 1), halts when it exits
└── kmain.c
```

### Build order

```text
headers (kernel uapi → sysroot) → libc (libk.a, libc.a, crt0.o) → kernel → user → grub (ISO)
```

The kernel includes its uapi headers through `-Iinclude/uapi`; `install-headers` copies only
`include/uapi/`, so user code and libc see `<mihos/...>` and never kernel internals. Kernel code uses
`CFLAGS` (`ARCH_CFLAGS`: `-mcmodel=kernel -mno-red-zone -mgeneral-regs-only`: no SSE/x87 in the
kernel, since a context switch saves only the general registers); ring-3 code (libc.a, `user/`) uses
`USER_CFLAGS` from `common.mk` (freestanding, no PIE, small code model, no unwind tables).
C++ (`.cpp` in programs and in `libc/cxx/`) is compiled with `$(CXX)` and `USER_CXXFLAGS`: the
same shared `USER_COMMON_FLAGS` plus `-std=gnu++20 -fno-exceptions -fno-rtti
-fno-threadsafe-statics` (no unwinder, no typeinfo, no `__cxa_guard_*`). Programs still link with
`gcc -nostdlib`.

### Include rules (enforced in `kernel/Makefile`)

| Code in | Can include |
|---|---|
| every directory | `include/` and `<asm/...>` |
| `arch/` and `platform/` only | additionally `<x86/...>` |

A generic file including `<x86/...>` fails to compile. Inline asm must not appear outside `arch/`.
Platform-private headers (`pic.h`, `vga.h`, `ps2.h`, `keyboard.h`, `multiboot2.h`) are quote-included
from the same directory.

### Naming

1. **Standards keep their names:** on-disk formats (ext2's `s_*`/`i_*`/`bg_*` fields, `rec_len`,
   `EXT2_FT_*`, `EXT2_MAGIC`, `EXT2_GOOD_OLD_REV`), the POSIX/ABI vocabulary (`ino`, `dev`,
   `rdev`, `mode`, `nlink`, `st_*`, `d_*`, `O_*`, errno names, `getdents`), and the usual OS words
   (`vfs`, `inode`, `superblock`, `mount`, `sector`, `block`). The spec and the man pages use
   the same words as the code.
2. **Our own names:** a module prefix (`ext2_`, `block_`, `block_cache_`, `vfs_`, `devfs_`,
   `initrd_`, `list_`/`hlist_`) and plain words after it; no abbreviation that is not in rule 1's
   vocabulary (`ext2_file_block`, not `bmap`; `ext2_volume_t`, not `ext2_fs_t`).
3. **Reference verbs, one meaning each:** `get` (a new reference), `put` (drop mine; the last one
   frees), `tryget` (a reference only if it is still alive), `release` (the callback after the
   last put), `free` (destroy, no counting). A function returning something "with a reference"
   hands its put to the caller.
4. **Intrusive links end in `_link`** (`child_link`, `device_link`, `lru_link`, `hash_link`,
   `cache_link`); list heads and tables are plural nouns (`children`, `nodes`). The embedded
   `vfs_node_t` in a file system's node struct is `base` (`n->base.size`).
5. **Lock fields name what they protect** (`position_lock`, `data_lock`; a plain `lock` guards
   the struct it is in).

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
`platform_irq_enable(irq)`, `platform_irq_eoi(irq)`, `platform_debug_write(s, n)`,
`platform_power_off()` (returns if it could not), plus a `screen` backend.

- **PC power off** (`platform/pc/power.c`): `SLP_EN` with sleep type 0 (S5) to the ACPI PM1a
  control register of both QEMU machines (`q35`: `0x604`; `pc`, the default: `0xB004`, where
  SeaBIOS puts the PM base), then `"Shutdown"` to Bochs's port `0x8900`. Real hardware needs the
  register from the ACPI tables (FADT): not yet, so it returns and the CPU halts.
- **Debug console** (`platform/pc/debugcon.c`): `platform_debug_write` copies bytes to port
  `0xE9`, which Bochs (`port_e9_hack: enabled=1`) and QEMU (`-debugcon`) send to the host.

### Boot sequence (`kmain`)

```text
tty_init → platform_boot_info_init → arch_init [TSS first] → input_init → platform_init (PIC, PS/2, keyboard IRQ, ATA disks)
→ mm_init(&boot_info) [mmap_init + kmalloc_init + arch_mmu_init] → initrd_init(&boot_info)
→ devfs_init [mounted on /dev] → mem_devices_init [/dev/null, /dev/zero]
→ scheduler_init [idle thread]
→ threads_init [reaper] → console_init(&keymap_abnt2) [/dev/console, console thread]
→ block_devfs_init [/dev/hda…: the disks platform_init found] → block_cache_init [64 buffers]
→ ext2_init [first ext2 disk on /mnt]
→ timer_init(TIMER_FREQUENCY_HZ) → irq_enable → guard_selftest → hashtable_selftest → mm_selftest
→ scheduler_selftest [+ sync tests] → console_selftest → user_selftest → vfs_selftest
→ elf_selftest
→ init_start(&boot_info) [starts /sbin/init, waits; init exiting = panic; init halts via SYS_REBOOT]
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
  A module ending above the direct map panics (`phys_to_kern` could not reach it). Today there is
  one module, the initrd.
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
  `USER_STACK_TOP` `0x00007FFFFFFFF000`, `USER_TOP` `0x0000800000000000` (canonical limit),
  `USER_STACK_SIZE` 64 KiB.
  ```text
  USER_BASE  text | rodata | data+bss | heap (heap_start .. brk, grows up) ...
             ... guard page | stack (USER_STACK_SIZE, grows down) | USER_STACK_TOP
  ```
- `address_space_t` (`mm/address_space.c`): `root`, `heap_start`, `brk`; `create` (zeroed: no heap
  until `exec_load`), `map` (fresh zeroed frames, `MMU_USER` added; on failure unmaps what it mapped,
  so nothing stays), `unmap` (translate, unmap, `frame_free`; skips holes), `write`/`read` (copy
  through the direct map, no activation needed), `activate` (NULL = kernel root), `destroy`.
  **Ownership rule:** everything mapped in the user half is freed with the space, so shared or kernel
  frames must never be mapped there. `heap_start`/`brk` have no lock (one thread per process, like
  the fd table).
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
  `SYS_WAIT` 5, `SYS_CLOSE` 10, `SYS_BRK` 11, `SYS_SPAWN` 12, `SYS_REBOOT` 13, `SYS_GETPID` 6, `SYS_GETPPID` 14, `SYS_OPEN` 15, `SYS_LSEEK` 16, `SYS_GETDENTS` 17, `SYS_STAT` 18, `SYS_FSTAT` 19, `SYS_IOCTL` 20, `SYS_FTRUNCATE` 21, `SYS_TRUNCATE` 22, `SYS_UNLINK` 23, `SYS_MKDIR` 24, `SYS_RMDIR` 25 (3, 4, 7–9 reserved). Read and write
  look the fd up in the process's table and call the file's ops; an empty slot, an out-of-range fd
  or a missing `FILE_READ`/`FILE_WRITE` flag gives `-EBADF`. Generic `sys/syscall.c` →
  `syscall_dispatch`; x86 `syscall.c` unpacks the frame. User pointers are `uintptr_t` in every
  `sys_*` function, never C pointers: only `address_space_read/write` touch them.
- **`spawn(path, argv, envp)`** → child pid or `-ENOENT`/`-ENOEXEC`/`-E2BIG`/`-EFAULT`/`-ENOMEM`.
  Copies into a `kmalloc`'d `spawn_args_t`: the path (≤ 64 bytes, `-E2BIG` beyond), then each
  vector (≤ 64 entries, all strings together ≤ 4 KiB; a NULL `envp` is an empty environment),
  byte by byte through `address_space_read` (`user_string_copy`, `user_vector_copy`).
  `vfs_read_file(path)` (a kmalloc'd copy; `-EISDIR` becomes `-EACCES`, as POSIX exec) →
  `exec_load` → `kfree` of the copy → `process_create` → `process_files_inherit` (same fd numbers, one file
  reference each) → link as a child (the `process_create` handle is the parent's reference) →
  `process_start`. Order matters: the child is complete before it can run.
- **`wait(pid, status)`** → the reaped child's pid, or `-ECHILD` (no matching child: not a child,
  already waited for, no children; `pid` 0 and `< -1` are process groups, which do not exist yet).
  `pid` -1 is any child. `process_wait_child` returns the child unlinked; `sys_wait` stores the POSIX
  status (`signal`, or `(exit_status & 0xFF) << 8`) if `status != 0` (`-EFAULT` after reaping on a
  bad pointer) and `process_release`s it.
- **`reboot(command)`**: only pid 1, else `-EPERM` (checked before the command); `REBOOT_HALT`
  (uapi `<mihos/syscall.h>`) → `system_halt()` (`sys/power.c`: the halt hooks, the block
  cache synced, IRQs off, `system halted`, `platform_power_off()`, `cpu_halt` if that
  returned), anything else `-EINVAL`. **Halt hooks** (`<power.h>`, Linux's reboot notifiers):
  a `halt_hook_t` (`run`, `link`) embedded in its owner, found back with `list_container`;
  `halt_hook_add` puts it at the front of a static list (boot and mount time only, no lock,
  never removed); `system_halt` runs them newest first, before the cache sync, so what they
  write goes out with it. libc:
  `<sys/reboot.h>`, `reboot(RB_HALT_SYSTEM)`.
- **`getpid()`** → the caller's pid (no lock: it never changes). **`getppid()`** → the parent's pid
  read under `process_tree_lock` (`process_parent_pid`), 0 without one (init, like Linux).
- **`brk(address)`** always returns the current break, never `-errno` (Linux semantics; libc turns
  "did not move" into `ENOMEM`). It moves only for `heap_start <= address <= limit`, `limit` =
  `USER_STACK_TOP - USER_STACK_SIZE - PAGE_SIZE` (a guard page above the stack); `brk(0)` is the
  query. Growing maps `[ALIGN_UP(brk), ALIGN_UP(address))` writable and keeps the old break on
  failure; shrinking unmaps and frees the pages above `ALIGN_UP(address)`. The break itself is exact
  to the byte. Processes not built by `exec_load` (self-tests) have `heap_start = 0`: growing fails
  on the range check.
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
- Self-test (`selftest/user_program.S`, `selftest/user.c`): one position-independent program
  copied to `USER_BASE`, started as a process with the console on fds 0–2: `loop` counts until
  the kernel sets `stop` (read and written through the address space), then exits 0. It proves a
  ring-3 loop that never makes a syscall is preempted, which no libc test can check (nothing in
  user space could stop a looping child). What the other raw programs checked is in the libc
  tests now: a `write` from a kernel address `EFAULT` (`files`), the `#PF` and `#GP` kills with
  `SIGSEGV` (`spawn`'s `segv` and `hlt` children), the fd table's `EBADF`s, closing twice and
  stdout after `close(2)` (`files`, `stdio`).

### Processes (`sched/process.c`)

```text
process_create(space) ──▶ process_fd_install(p, file) ×N ──▶ process_start(p, entry, stack)
     refs = 1 (handle)                                          refs++ (thread), then the thread runs
```

- `process_t`: pid, state (`RUNNING`/`EXITED`), `exit_status`, `signal`, `refs`, `space`, `thread`
  (one per process), `parent`, `children`/`sibling`, `waiters`, `child_exits`, `files[16]`.
  Allocated with `kzalloc`.
- **Parent and children:** `parent` is NULL for processes the kernel started (they are not on any
  list; their creator holds the handle). `children` holds spawned, not yet waited-for children, one
  reference each (`process_child_add` at spawn, `process_child_remove` if it cannot start). An
  exited child stays (a zombie) until `wait`. `parent`, `children` and `sibling` of every process are
  protected by the global `process_tree_lock` (taken *before* a `p->lock`, never inside one).
- **Waiting for children** (`process_wait_child(parent, pid, &child)`): one Mesa loop under the tree
  lock scans `children` for a match (`pid`, or any for -1): an exited match is unlinked, its
  `parent` cleared, and returned (the caller now owns the reference); matches all running → sleep
  on the parent's `child_exits` and scan again; no match → `-ECHILD`. `process_exit` sets `EXITED`,
  then wakes `parent->child_exits` under the tree lock. A child dereferences `parent` only under that
  lock, and the parent clears it (wait, exit) under the same lock, so a woken parent is never freed.
- **Orphans go to init:** `process_create_init` remembers init (`init_process`, never cleared).
  In the same tree-lock section that wakes its own parent, `process_exit` moves every remaining
  child onto init's `children` with its reference (`parent = init`): a running one wakes init when
  it exits, an already exited one makes `process_exit` wake `init->child_exits` right away. Init
  reaps them in its `wait(-1)` loops. Without an adopter (before init exists: kernel self-tests;
  or init itself exiting, which panics next) children are dropped: moved to a local list,
  `parent` cleared, `process_release`d after the lock (one still running is freed when it exits,
  one already exited right there: its space is not the active one).
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
- `process_wait(p)` (kernel callers waiting for a parentless process: `init_start`, self-tests):
  Mesa loop on `waiters` until `EXITED` (`wait_queue_sleep_locked` on `p->lock`),
  returns `exit_status`. The struct and space stay valid until the caller's `process_release`, so tests
  can read the program's variables.
- **Locking:** `p->lock` protects `refs`, `state` and `exit_status`; `process_exit` sets the status
  and wakes waiters under it. The last release and the teardown run *after* the lock is dropped, never
  freeing a held lock. pids come from `pid_lock`, starting at 2: `PROCESS_INIT_PID` (1) is reserved
  for `/sbin/init` (`process_create_init`), since Unix programs assume init is pid 1.
- **Exit status:** `exit_status` is the value passed to `exit` (full `int`), or 128 + signal for
  faults (`uapi/mihos/signal.h`: Linux signal numbers, no delivery yet); the kernel self-tests and
  the runner compare it. Faults go through `process_exit_signal(sig)`, which also records `signal`,
  so `wait` can encode "killed by a signal" apart from "exited 139". Only `wait` truncates to 8 bits.
- The fd table has no lock: with one thread per process only the creator (before start) and the
  process itself touch it. Needs one with threads or `fork`.

### Files (`fs/file.c`)

```text
process->files[fd] ──▶ file_t { ops, flags, refs, private, node, offset } ──▶ ops->read / write / release
```

- An fd is an index into the process's table; a `file_t` is an open file (POSIX open file
  description) that several fds can share. `file_ops_t` takes **kernel** buffers and returns bytes or
  `-errno`; the syscall layer does every user copy, so backends never see user pointers.
- `file_create(ops, flags, private)` (refs = 1), `file_get`, `file_put` (at 0: optional `release`,
  then the node put if `node` is set, then `kfree`: the file owns that reference whatever its
  ops are, since a device's driver knows nothing about nodes). Refs are protected by the file's embedded `lock`; `release` runs without locks held,
  once, after the last reference is gone. `file_create` also sets `node = NULL`, `offset = 0` and
  initializes `position_lock`.
- **Position:** `node` is the `vfs_node_t` a VFS file reads (one reference, NULL for devices like
  the console), `offset` the next byte. `offset` is protected by `position_lock`, a **mutex** held
  across a read and its offset update (`vfs_file_read`) and across `lseek`: an open file can be
  shared (fds inherited by `spawn`), and two readers must never get the same bytes. A mutex, not
  the spinlock, because a disk read will sleep (Linux's `f_pos_lock`).
- **VFS files** (`fs/vfs.c`): `vfs_open(path, flags, &file)`: unknown flags → `-EINVAL`, then
  `vfs_lookup` (so a missing path is `-ENOENT` before any access check), `O_DIRECTORY` on a
  non-directory → `-ENOTDIR`, then the access for every node, a value in `flags & O_ACCMODE`
  (`O_RDONLY` 0 → `FILE_READ`, `O_WRONLY` → `FILE_WRITE`, `O_RDWR` → both, 3 → `-EINVAL`;
  `O_RDONLY` is 0, so it cannot be tested as a bit). **A device** (`VFS_CHARDEV`):
  `file_create(node->device_ops, access)`. **Anything else** (files, directories, disks): write
  access to a directory → `-EISDIR`, to a node without a `write` op → `-EROFS` (the initrd,
  ext2 until W4), then `file_create(&vfs_file_ops, access)`: exactly the access asked for, so
  an `O_WRONLY` file reads `-EBADF`. Either way the lookup's reference moves into
  `file->node`. `vfs_file_ops`: `read` = `vfs_read` at `offset`, `write` = `vfs_write` at
  `offset` (`vfs_file_write`, the same `position_lock`), each advancing it by what it moved; no
  `release` (`file_put` puts the node). `vfs_write`: a directory → `-EISDIR`, no `write` op →
  `-EROFS`; the op returns the count written (fewer at the end of a disk) or `-errno`.
  **`O_TRUNC`** (`0x200`): accepted by `vfs_open`; after the file is made (and holds the node),
  a regular file opened with write access is `vfs_truncate`d to 0 (a failure `file_put`s it);
  ignored without write access and on devices. **`vfs_truncate(node, size)`**: a directory →
  `-EISDIR`, a character or block device → `-EINVAL` (its size is the device's), no `truncate`
  op → `-EROFS`, else the op. **`ftruncate(fd, length)`** (`sys_ftruncate`): `-EBADF`; not open
  for writing, no node or a negative length → `-EINVAL` (Linux); the offset never moves.
  **`truncate(path, length)`**: a negative length `-EINVAL`, `user_path_copy`, `vfs_lookup`,
  `vfs_truncate`. libc: `ftruncate`/`truncate` in `<unistd.h>`. **`O_CREAT`** (`0x40`) /
  **`O_EXCL`** (`0x80`), Linux's values: `vfs_open(path, flags, mode, &file)` takes the
  permissions of a created file. A lookup giving `-ENOENT` with `O_CREAT` → `vfs_create(path,
  mode)`: a trailing `/` → `-EISDIR`; the last component is the name (a pointer into the path
  and a length, over `VFS_NAME_MAX` → `-ENAMETOOLONG`), the rest (its `/` kept) is copied into
  a `kmalloc`'d string and looked up (through mounts); not a directory → `-ENOTDIR`, no
  `create` node op → `-EROFS` (initrd, devfs), else `create(dir, name, length, mode & 07777,
  &node)`. A create giving `-EEXIST` without `O_EXCL` (made meanwhile) is retried as a lookup
  once: a name the VFS cannot show (a symlink) is `-EEXIST` to create and `-ENOENT` to look up,
  so a loop would never end. An existing file with `O_CREAT | O_EXCL` → `-EEXIST`; `O_EXCL`
  alone is ignored (POSIX leaves it undefined, Linux ignores it). The new node then goes
  through the usual checks (`O_TRUNC` on it is a no-op). No umask. libc: `open` reads its
  `mode_t` with `va_arg` only with `O_CREAT`; `creat(path, mode)` = `open(path, O_WRONLY |
  O_CREAT | O_TRUNC, mode)` (an existing file keeps its mode). **`vfs_unlink(path)`**
  (`sys_unlink`, libc `unlink`): a path ending in `/` is looked up whole (a file → the
  lookup's `-ENOTDIR`, a directory → `-EISDIR`); else `vfs_lookup_parent` (shared with
  `vfs_create`: the last component as a pointer and a length, the parent looked up through
  mounts, `-EISDIR` for a trailing `/`, `-ENAMETOOLONG`, `-ENOTDIR`), then the parent's
  `unlink` node op (`-EROFS` without one: initrd, devfs). A name is removed; the file goes
  when it has no names and no references (an open file stays usable). **`O_APPEND`**
  (`0x400`, Linux's 02000): a regular file's open gets `FILE_APPEND`; `vfs_file_write` then
  passes `VFS_APPEND` (`UINT64_MAX`) as the offset, which the file system turns into the
  size under its own lock (two appenders never write at the same place), and sets the
  offset to the size after the write. Ignored on devices and disks. **`vfs_mkdir(path,
  mode)`** / **`vfs_rmdir(path)`** (`sys_mkdir`/`sys_rmdir`, libc `mkdir` in `<sys/stat.h>`,
  `rmdir` in `<unistd.h>`): a trailing `/` suits both (`vfs_path_trimmed` copies the path
  without it, `"/"` staying `"/"`), then `vfs_lookup_parent` and the parent's `mkdir`/`rmdir`
  node op (`-EROFS` without one). `mkdir` of a path that already looks up → `-EEXIST` first
  (also `/` and the initrd's directories). `rmdir`: `"/"` → `-EBUSY`, a last name `.` →
  `-EINVAL` (POSIX), `..` → `-ENOTEMPTY` (Linux), decided by the VFS so a file system is
  never asked to remove a directory from itself (ext2 would lock the same node twice). Directories open (listed with
  `getdents`); reading one gives `-EISDIR`. `vfs_read`: a directory → `-EISDIR`, a node without
  a `read` op (character devices, read through their `file_ops`) → `-EINVAL`. A block device is
  read like a file: `lseek` works on it, since only `VFS_CHARDEV` is refused.
- **`open(path, flags, mode)`** (`sys_open`, the mode passed on to `vfs_open`): the path copied into a `kmalloc`'d `VFS_PATH_MAX` (4096)
  buffer (`-E2BIG` → `-ENAMETOOLONG`, `-EFAULT`), freed on every path; `vfs_open` (`O_DIRECTORY`:
  a file gives `-ENOTDIR`, for `opendir`; without it directories and files both open);
  `process_fd_install` (lowest free fd, `-EMFILE` → `file_put`, which frees the file and puts the
  node). **`lseek(fd, offset, whence)`** (`sys_lseek`): `-EBADF`, `-ESPIPE` without a node or on
  a character device (Linux lets `/dev/null` "seek"; one rule is simpler); under
  `position_lock`, base 0 / `offset` / `node->size` (bad whence → `-EINVAL`); a result below 0 is
  `-EINVAL`, checked before adding (`offset < 0 && -(offset + 1) >= base`); past the end is allowed
  (reads give 0). `read` and `close` are unchanged: `sys_read` reads one 256-byte chunk per call (a
  short read).
- **`getdents(fd, buffer, size)`** (`sys_getdents`): `-EBADF`, `-ENOTDIR` without a node (the
  console) or on a file. A directory's `offset` is an **entry index** (not bytes), so `lseek` to 0
  rewinds it. `size` is capped at a page (a short result) and the records are built in a
  **`kzalloc`'d** buffer (the padding is copied out too: no stale kernel bytes). Under
  `position_lock`: `vfs_file_getdents(file, buffer, size, &next)` (in `vfs.c`, never sees user
  pointers) packs entries from `offset` on, `address_space_write`, and only then `offset = next`,
  so `-EFAULT` loses no entries. Record (uapi `<mihos/dirent.h>`, `struct mihos_dirent`):
  `d_ino` (u64), `d_reclen` (u16), `d_type` (u8: `MIHOS_DT_CHR` 2 / `MIHOS_DT_DIR` 4 / `MIHOS_DT_BLK` 6 /
  `MIHOS_DT_REG` 8), `d_name`
  `'\0'`-terminated at offset 11; `d_reclen = ALIGN_UP(11 + length + 1, 8)` so every `d_ino` is
  8-aligned. Returns bytes, 0 at the end, `-EINVAL` if the first record does not fit (Linux).
- **`stat(path, st)` / `fstat(fd, st)`** (`sys_stat`, `sys_fstat`): `stat` copies the path with
  `user_path_copy` (shared with `sys_open`: a `kmalloc`'d `VFS_PATH_MAX` buffer, `-E2BIG` →
  `-ENAMETOOLONG`), `vfs_lookup`s it, `vfs_stat`s into a kernel `struct mihos_stat`, puts the
  node, then `address_space_write`s it (`-EFAULT`). `fstat`: `-EBADF`, then `vfs_stat` of
  `file->node` (every open file has one; node-less files such as pipes will need a decision).
  The ABI (uapi `<mihos/stat.h>`): fixed-width fields with an explicit `__pad0`, Linux's x86-64
  `struct stat` layout; `MIHOS_S_IFMT`/`IFDIR`/`IFCHR`/`IFBLK`/`IFREG` (octal, Linux values) in the high
  bits of `st_mode`, permissions in the low 12. **`vfs_stat(node, st)`** zeroes everything, then
  `st_ino`, `st_mode` = type bits | `node->mode`, `st_nlink` = `node->nlink` (1 for the initrd and devfs; also what tools
  read as "not counted" for directories), `st_size` (0 for character and block devices, as on
  Linux: a disk's size comes from `lseek(SEEK_END)`, which uses `node->size`), `st_blksize` =
  `PAGE_SIZE`, `st_blocks` in 512-byte units (0 for devices), all three times = `node->mtime` (read-only, no clock), `st_dev` = `node->dev`,
  `st_rdev` = `node->rdev`; `st_uid`, `st_gid` 0.
- **Console backend** (`driver/console.c`): `console_init` registers `console` in devfs (5:1,
  `0600`, Linux's numbers and mode) with the console's `file_ops`, then `vfs_open`s
  `/dev/console` read-write for itself and keeps that reference forever. `console_file()`
  returns a new reference to that one file; init gets it three times, as fds 0, 1 and 2 (one
  open file shared, as if `dup`ed: Linux opens `/dev/console` once for init and dups it).
  Its `ioctl` answers `TIOCGWINSZ` (`SCREEN_HEIGHT` × `SCREEN_WIDTH`, no pixels) and gives
  `-ENOTTY` for anything else, like Linux's tty layer.
- **`ioctl(fd, request, arg)`** (`sys_ioctl`): `-EBADF`; no `ioctl` op in the file's ops →
  `-ENOTTY` (initrd files, directories, `/dev/null`, `/dev/zero`). A request encodes its
  argument (uapi `<mihos/ioctl.h>`, Linux's asm-generic `_IOC` layout: bits 31–30 direction,
  `MIHOS_IOC_WRITE` 1 user → kernel, `MIHOS_IOC_READ` 2 kernel → user; 29–16 size; 15–8 a type
  letter; 7–0 a number; `MIHOS_IOR`/`MIHOS_IOW` build them), so the syscall layer does every
  copy: a zeroed `IOCTL_ARG_MAX` (256) stack buffer (bigger → `-EINVAL`), copied in for WRITE,
  the op called with that kernel buffer, copied out for READ only when it returned `>= 0`
  (`-EFAULT`). Requests: `MIHOS_TIOCGWINSZ` = `MIHOS_IOR('T', 0x13, struct mihos_winsize)`
  (`ws_row`, `ws_col`, `ws_xpixel`, `ws_ypixel`, `uint16_t` each).

### Programs and the ELF loader (`exec/`, `init.c`, `user/`)

```text
user/tests/hello ─build─▶ sysroot/usr/tests/hello ─grub/Makefile─▶ initrd.tar (usr/tests/hello)
  ─ISO /boot/initrd.tar─▶ module "initrd" ─▶ initrd_init: tree of nodes, mounted as "/"
  ─▶ vfs_lookup("/usr/tests/hello") / vfs_read_file (kmalloc'd copy)
init_start ─▶ vfs_read_file("/sbin/init") ─▶ argv = {"/sbin/init", words of the cmdline} ─▶ exec_load
           ─▶ process_create_init (pid 1) + console fds 0-2 + start ─▶ process_wait ─▶ halt
/sbin/init ─spawn─▶ /usr/tests/run ─spawn─▶ each test ─waitpid─▶ compare with its table
```

- **User build** (`user/Makefile`): every directory under `bin/`, `sbin/` and `tests/` is a program
  (discovered, no list), one static ELF each, linked as `crt0.o` (first: holds `_start`) + the
  program's objects + `-lc -lgcc`, with `USER_CFLAGS` and `max-page-size=4096`. `user/include/` is on
  the include path (shared headers such as `test.h`). `user.ld` puts text (RX), rodata (R) and
  data+bss (RW) in separate `PHDRS` segments, each on its own page at `USER_BASE`.
- **Install by category:** `bin/x` → `<sysroot>/bin/x`, `sbin/x` → `<sysroot>/sbin/x`,
  `tests/x` → `<sysroot>/usr/tests/x` (the build tree keeps `.elf`; installed programs have none).
- **Initrd build** (`grub/Makefile`): `bin/`, `sbin/` and `usr/tests/` of the sysroot (not headers
  or libraries; leftover `*.elf` skipped) are copied to a staging folder and packed with
  `tar --format=ustar --owner=0 --group=0 --numeric-owner --sort=name`, together with the empty
  `MOUNT_POINTS` directories (`dev/`, mode 0755). The top-level operands go through
  `$(sort ...)`: `--sort=name` only orders what is inside each one, and the initrd lists
  entries in archive order (the tests expect `bin dev sbin usr`). The Makefile is a
  prerequisite of the archive, so a layout change repacks it. The ISO is rebuilt from
  scratch: `/boot/mihos.kernel`, `/boot/initrd.tar`, `/boot/grub/grub.cfg`.
- **`grub.cfg`**: each entry is `multiboot2 /boot/mihos.kernel <init's arguments>` (GRUB passes
  the words after the kernel path as the command line, without the path) and
  `module2 /boot/initrd.tar initrd`. A new program needs no line.
- **Modules** (`boot_info.modules[]`, Multiboot2 tag 3): physical `[start, end)` plus the whole module
  string as `name`, copied with `strlcpy` (the Multiboot2 structure is not protected after boot).
- **Initrd** (`fs/initrd.c`): `initrd_init(info)` finds the module named `initrd` (first word of
  its string), then walks every ustar header once: magic `ustar`, checksum (the 512 bytes summed
  with the checksum field counted as spaces), octal size (`tar_parse_octal`: optional leading spaces,
  digits up to a NUL/space), and data inside the module (`size > initrd_size - data` → panic). A
  corrupt archive panics, so the build trusts it: `tar_next` steps header + data rounded to 512,
  stopping at a zero block. Then the tree: `initrd_node_t` embeds a `vfs_node_t` (`INITRD_NODE`
  = `list_container`) plus a `kmalloc`'d name, `data` (files: a pointer into the archive, never
  copied or freed) and `children`/`child_link` lists. `initrd_add_entry` rebuilds each entry's path
  (`prefix/name`, `%.*s` since a full 100-byte field has no NUL), walks it with `vfs_path_next`
  looking one component ahead: every component before the last is a directory, found or created
  (`initrd_find_or_create_dir`, `mkdir -p`: tar lists `usr/tests/` but never `usr/`), the last is the file
  (type `'0'`/`'\0'`) or directory (`'5'`); other types are skipped; a name that is both a file
  and a directory, or a file listed twice, panics. The root is its own parent; `vfs_mount_root`.
  Ops: `lookup` (linear search of `children`, returns a new reference), `read` (copies
  `min(count, size - offset)` from `data`), `readdir` (index 0 `.` = the node, 1 `..` =
  `parent`, then the children in archive order, walking the list from the start each call), no
  `release` (the tree lives for the whole run). Each entry's `mode` (`& 07777`) and `mtime`
  (both octal, `tar_parse_octal`; bad → panic) go to the node the entry describes, set after the path
  walk, so a directory made by "mkdir -p" (`usr/` before `usr/tests/`) gets its own values when
  its `'5'` entry comes; directories never listed (`/usr`) and the root get `0755`, mtime 0.
  Inode numbers come from a counter in
  `initrd_node_create` (root 1); a name longer than `VFS_NAME_MAX` panics.
- **VFS** (`fs/vfs.c`, `include/fs/vfs.h`):
  ```text
  fd ─▶ file_t (open file: ops, flags; offset later) ─▶ vfs_node_t (the file) ─▶ file system
  ```
  `vfs_node_t`: `type` (`VFS_FILE`/`VFS_DIRECTORY`), `size`, `ino` (inode number), `mode`
  (permission bits only; `vfs_stat` adds the type), `mtime` (seconds since 1970, 0 unknown),
  `dev` (the file system's id from `vfs_dev_alloc`, never 0; `st_dev`), `mounted_root` (on a mount
  point: the root mounted there), `mount_point` (on a mounted root: the directory it hides), `ops`,
  `parent` (for `..`), `refs` under `lock`; no name: a name belongs to the directory entry, not
  the file (hard links later). `vfs_node_ops_t`: `lookup(dir, name, length, &child)` (name not
  terminated: a pointer into the path), `read(file, buffer, count, offset)` (bytes, 0 at the end,
  or -errno), `readdir(dir, index, &entry)` (entry number `index` into a `vfs_dirent_t` {`ino`,
  `type`, `length`, `name[256]`}: 1, 0 past the last, or -errno; each file system emits `.` and
  `..` itself at 0 and 1, as ext2 stores them on disk; no nodes are created for a listing),
  `release` (after the last `put`; NULL: never freed). `vfs_node_put` decrements under the lock
  and calls `release` after it. `vfs_node_tryget` takes a reference only if there is one (a
  cache's lookup: at 0 the release is on its way, so a node never comes back from 0 and each
  `release` runs once; Linux's `igrab`). `nlink` (1 unless the file system counts links, as
  ext2 does) goes to `st_nlink`. `vfs_path_next(&p, &length)`: skips any `/`, returns a component
  as pointer + length, no copy (a non-writing `strtok_r`). `vfs_lookup(path, &node)`: absolute
  paths only (relative or `""` → `-ENOENT`: no current directory yet); holds exactly one
  reference per step (gets the next node, puts the current one, a single `put` on every exit);
  `.` keeps the node, `..` takes `parent`; a component longer than `VFS_NAME_MAX` (255) →
  `-ENAMETOOLONG`, a file before another component → `-ENOTDIR`, a file with a trailing `/` →
  `-ENOTDIR` (POSIX). Returns a new reference. `vfs_read` checks the type (`-EISDIR`),
  `vfs_readdir` too (`-ENOTDIR`).
  `vfs_read_file(path, &data, &size)`: the whole file in a `kmalloc`'d buffer (loops on `read`;
  0 before the end → `-EIO`), the outputs set only on success. Open files: see Files.
  **Mounts:** `vfs_mount(path, root)` looks the directory up (`-ENOENT`/`-ENOTDIR` pass
  through; a file → `-ENOTDIR`; `/` or an already mounted directory → `-EBUSY`: lookup has
  crossed into the existing root, so "mounted" shows as landing on a node with `mount_point`), then
  `point->mounted_root = root`, `root->mount_point = point`, and keeps a reference on both forever (no
  unmount; mounts happen at boot, so no lock). `vfs_lookup` crosses **down** after every step
  (`while (node->mounted_root)`: get the root, put the point; stacked mounts work), and `..` first
  climbs **out** (`while (up->mount_point) up = up->mount_point`, no references needed: pinned by the
  mounts), then takes `parent`. Readdir does not follow mounts: `/` lists the initrd's `dev`
  (its inode), and devfs's `..` is its own root, as on Linux.
  ```text
  initrd /            devfs (st_dev 2)
  ├── bin/ sbin/ usr/
  └── dev/ ─mounted_root─▶ (root, ino 1) ─mount_point─▶ dev/
  ```
- **devfs** (`fs/devfs.c`, `include/fs/devfs.h`): `devfs_init()` right after `initrd_init`
  takes the second dev id, makes a static root (`VFS_DIRECTORY`, ino 1, `0755`, its own
  parent, never released) and `vfs_mount`s it on `/dev` (panics if that fails). `lookup` →
  `-ENOENT` and `readdir` → `.`/`..` only, until devices register (step (b)).
  **Devices:** `devfs_register(name, mode, rdev, &file_ops)` (boot time, no lock): `NULL`,
  empty, over `VFS_NAME_MAX` or containing `/` → `-EINVAL`, already there → `-EEXIST`; a
  `kzalloc`'d `devfs_node_t` (embeds the `vfs_node_t`, plus a `kmalloc`'d name and a `device_link`
  link) of type `VFS_CHARDEV` with devfs's `dev`, the next inode (from 2), `mode & 07777`,
  `rdev`, `device_ops` = the driver's ops, parent the root, appended to `devfs_devices`. Device
  nodes have an all-NULL ops table (the VFS never looks up, reads or lists through a device;
  never freed). `lookup` and the duplicate check share `devfs_find`; `readdir` lists the dots
  (directories), then the devices from index 2 in registration order.
- **Character devices** (`VFS_CHARDEV`): `vfs_node_t.rdev` (`MIHOS_MKDEV(major, minor)`: major
  in the high 32 bits, uapi `<mihos/stat.h>`) and `device_ops` (Linux's `i_fop`). `vfs_stat`:
  `S_IFCHR` + `st_rdev`, size 0; `getdents`: `MIHOS_DT_CHR` (2). Two helpers map the node type,
  `vfs_mode_type` (`st_mode` bits) and `vfs_dirent_type` (`d_type`): the two number the types
  differently. `read`/`write` need nothing new: they dispatch through `file->ops`.
- **Memory devices** (`driver/mem.c`, Linux's major 1): `mem_devices_init()` after
  `devfs_init` registers `null` (1,3: reads 0, swallows writes) and `zero` (1,5: reads zeros,
  swallows writes), both `0666`; failing panics. The drivers see kernel buffers only
  (`sys_read`/`sys_write` copy in 256-byte chunks).
- **`elf_load(space, image, size, &entry, &end)`** (`exec/elf.c`): validates `ELF64`, little-endian,
  version, `ET_EXEC`, `e_machine == ELF_MACHINE` and that the program header table is inside the
  image; for every non-empty `PT_LOAD` (empty ones, e.g. an unused RW segment, are skipped) checks the
  file range and that memory lies in `[USER_BASE, USER_TOP)`, maps whole pages (`PF_W`/`PF_X` →
  `MMU_WRITE`/`MMU_EXEC`; readable is implied) and copies `p_filesz` bytes to `p_vaddr` (`.bss` stays
  zero: frames are zeroed). The entry must be inside an executable segment. Returns `-ENOEXEC` /
  `-ENOMEM`, sets `entry` and `end` (page-aligned end of the highest segment) only on success, never
  creates a process. On error the caller destroys the
  space. Every bound is a subtraction after a `<=` check; `p_vaddr > USER_TOP` is rejected **before**
  `USER_TOP - p_vaddr`, which would wrap and let a segment map into the kernel half.
- **`exec_load(image, size, argv, envp, &space, &entry, &stack)`** (`exec/exec.c`): new space +
  `elf_load` + the stack (`USER_STACK_SIZE` below `USER_STACK_TOP`) + `stack_build`; on success
  `heap_start = brk = end`, so the heap starts empty on the page after `.bss`. `argv`/`envp` are
  kernel vectors of kernel strings (constants from the runner, copies from `spawn`). Frees
  everything on error.
- **Initial stack** (`stack_build`, System V x86-64):
  ```text
  USER_STACK_TOP  argv and envp strings (packed, in order)
                  padding
                  0, 0                 auxiliary vector: AT_NULL only
                  NULL, envp[envc-1] .. envp[0]
                  NULL, argv[argc-1] .. argv[0]
  rsp (16-aligned) argc
  ```
  Strings at the top, `sp = ALIGN_DOWN(strings - words * 8, 16)`; the pointer vector is built in a
  `kmalloc`'d buffer (string addresses are known while writing the strings) and written once.
  Everything ≤ `ARGS_MAX` (a quarter of the stack) or `-E2BIG`.
- **`init_start(info)`** (`init.c`): `vfs_read_file("/sbin/init")` (error → panic; the copy is
  never freed: `program_run` returns only when init exits, then the kernel panics), copies the
  kernel command line into a `BOOT_CMDLINE_MAX` buffer, `argv[0] = "/sbin/init"`, then splits the
  copy in place at spaces into `&argv[1]` (`args_split`, at most `INIT_ARGS_MAX - 1` words; 8
  entries with the path), `program_run`s it (cannot start → panic) and panics if it ever returns
  (`init exited with status N`): init stops the system only through `SYS_REBOOT`.
  `program_run(image, size, argv, &status)` = `exec_load` (`envp = {"PATH=/bin"}`) +
  `process_create_init` + console on fds 0–2 + start at the built stack + wait + release; returns a
  negative errno (cleaning up) when the program cannot start.
- **`/sbin/init`** (`user/sbin/init`, pid 1): `argv[1]` is the mode, `&argv[2]` the command
  (already NULL-terminated), started with init's `envp`. Never returns from `main`: everything ends
  in `halt()` (`fflush`, `reboot(RB_HALT_SYSTEM)`; if refused, prints `errno` and spins).
  - `once`: start, `wait(-1)` until the command's pid comes back (other pids are reaped orphans),
    print `init: <command> exited with status N` (or `killed by signal N`), halt.
  - `respawn`: the same wait loop, then start again: quietly after an exit (`exit`, Ctrl+D: a new
    shell, like a new login), with a message after a kill; `CRASHES_MAX` (5) kills in a row
    (reset by a clean exit) → `keeps crashing, giving up` and halt.
  - Unknown mode or no command → usage line and halt. Failing `start` or `wait` → halt.
  - Orphans the kernel hands to init come back through the same `wait(-1)` loops and are reaped.
- **`/usr/tests/run`** (`user/tests/run`): the automatic set is every regular file in
  `/usr/tests` (found with `readdir`, sorted by name with an insertion sort) except `.`-names,
  `run` and the `interactive[]` names, which run only with the `interactive` argument. Expected
  status 0 unless listed in `special[]` (`hello` 42, `malloc_abort` 134, `cxx` 42); a new test
  needs no line. Names are `strdup`'d and the directory closed **before** the first spawn (tests
  inherit fds; `files`/`dirs` expect fd 3 free). An unopenable or empty directory, more than
  `TESTS_MAX` (64) tests, or a `special[]` name not found is a `FAIL` (so `0 passed, 0 failed`
  never passes). Spawns each test with `argv = {path}` and `environ`, passes
  when `WIFEXITED` and `WEXITSTATUS == expected` (so expected statuses are 0..255), prints
  `test <path>: ok` / `FAIL (...)` and `tests: N passed, M failed`, exits with the failures.
- **Test programs** (`user/tests/`, `user/include/test.h`): `CHECK(expr)` prints
  `FAIL line N: expr` to stdout and counts; `main` returns `test_status()` = failed checks.
  - `hello`: prints nothing, returns `status + zero` from a `.data` 42 and a `.bss` 0 (expected 42: checks the copy
    and the zeroing).
  - `stdio`: buffering order checked by eye (`1`, `3`, `2 no newline... 4`, `checks: …`,
    `5 flushed by exit` last, only via `exit`), return values, `ferror` + `EBADF` after `close(2)`.
  - `brk`: heap on the page after `.bss`, exact unaligned break, zeroed pages after shrink and regrow,
    `ENOMEM` past the limit / on wrap / below `heap_start`, a 32 KiB stack array.
  - `malloc`: 16-alignment and no overlap over 15 sizes, one free block after freeing everything
    (`malloc(heap - header)` fits at the bottom without moving the break), 1000 rounds without heap
    growth, a 100 KiB block, `calloc` zeroing reused dirty memory, `realloc` semantics, `ENOMEM`
    cases.
  - `malloc_abort`: double free (expected 134).
  - `stdin`: pushback (incl. a negative `char` coming back as 0..255, `fgets` with `n` 2 and 1),
    `fgetc(stdout)` → `EOF` + `ferror`, closed stdin → `EOF` + `EBADF` + `ferror`, `fgets` NULL.
  - `stdin_interactive` (second GRUB menu entry, picked within the 5-second timeout): prompts
    without `'\n'` (seeing them checks the flush before `read`), a whole line, a line split over
    `fgets` calls, `getchar`, `abc` + Ctrl+D twice → `"abc"` then `feof`, sticky EOF, `clearerr`.
  - `args`: `argc == 1`, `argv[0]` its path, `envp = {"PATH=/bin"}`, `environ == envp`, vectors
    and strings on the stack, a 16-aligned frame in `main`.
  - `reboot`: `reboot` from a non-init process returns `EPERM`, also for an unknown command (a
    missing check halts the machine mid-run instead).
  - `cxx` (C++): global constructors in declaration order before `main`, a local static built
    once, virtual calls and `delete` through a virtual destructor, 16-aligned `new`,
    `new[]`/`delete[]` over 10 objects, an `atexit` handler from `main` running before the global
    destructors, which run in reverse; the last one calls `_exit(42)` (1 on a failure), so
    missing destructors show as `main`'s 0. Expected status 42.
  - `spawn`: spawns itself with a mode in `argv[1]` (no extra modules): exit 7 / 300 (→ 44) /
    SIGSEGV through the `W*` macros, `argv`/`envp` passed exactly, NULL `envp`, two children waited
    in reverse order, waiting twice → `ECHILD`, 20 rounds, `ENOENT`/`EFAULT`/`E2BIG`/`ENOSYS`/
    `ECHILD`/`EINVAL`, `EACCES` for a directory, `ENOTDIR` for `hello/`, `ENOENT` for a
    relative path, an orphaned grandchild (adopted by init, not waitable), a child's read on an
    inherited fd moving the parent's offset (10), fd numbers kept across a closed fd
    0. `wait`/`waitpid(-1)`: `ECHILD` with no children, a still-running child (sleep and rescan),
    three children reaped once each with their own status, `waitpid(pid)` then `wait` for the
    other, `ECHILD` for `pid` 0 and -2 with a child alive.
  - `files`: `open` of `/usr/tests/hello` gets fd 3 and reads the ELF header; reads continue at
    the offset, return 0 at the end (again), pieces of 7 or 300 bytes add up to `lseek(SEEK_END)`;
    `lseek` from each origin, `SEEK_CUR` 0 as "where am I", the same byte from the start and from
    the end, past the end reads 0; `EINVAL` (before 0, `INT64_MIN`, bad whence) leaves the offset;
    `ESPIPE` on stdin, `EBADF` on a bad fd; two opens have independent offsets; `open` errors
    `ENOENT` (missing, relative), `ENOTDIR`, `EROFS` (`O_WRONLY`/`O_RDWR`), `EINVAL` (unknown
    flag), `EFAULT`, `ENAMETOOLONG` (5000 bytes); `write` to a read-only fd → `EBADF`; a directory
    opens but reads `EISDIR`; a closed fd → `EBADF`; 13 opens fill the 16-entry table, then
    `EMFILE`, and after closing `open` gets 3 again.
  - `dirs`: `/usr/tests` lists `.` then `..`, `hello`/`run`/`dirs` once each, `DT_DIR` for the
    dots and `DT_REG` for the rest, NULL with `errno` 0 at and past the end; `rewinddir` gives
    the same listing; inode numbers agree across directories (`.` of `/usr/tests` = `tests` in
    `/usr`, `..` = `.` of `/usr`, the root's dots equal); `opendir` `ENOTDIR` (a file, `hello/`),
    `ENOENT` (missing, relative); `open` without `O_DIRECTORY` takes both kinds. Raw `getdents`
    on `/`: `EINVAL` into 8 bytes, exactly one 16-byte record, the other 64 bytes, 0 twice;
    `EFAULT` after `lseek` 0 loses nothing (80 bytes next); zero padding; 40-byte buffers resume
    and add up to `readdir`'s count; `ENOTDIR` on stdin and a file, `EBADF` on closed and
    unopened fds; fd 3 free again at the end.
  - `stat`: `hello` is `S_ISREG`, `0755`, one link, blocks = size/512 rounded up, `st_mtime`
    (the macro) = `st_mtim.tv_sec` ≥ 2023 and equal to the other two times; size = `lseek`
    end, inode = `readdir`'s `d_ino`, `fstat` of the open file byte-identical to `stat` (both
    zeroed first: `struct stat` has padding after `st_mode`), two paths one inode. Directories
    (`/usr/tests`, with a trailing `/`, `/`, the never-listed `/usr`) `S_ISDIR` and `0755`;
    `fstat` of an open directory; fds 0–2 `S_ISCHR` with `/dev/console`'s inode, `st_dev` and
    `st_rdev` (5:1, `0600`). `ENOENT` (missing, relative), `ENOTDIR`,
    `ENAMETOOLONG`, `EFAULT` (path), `EBADF` (99, -1, closed). A bad `struct stat *` is not
    tested: libc copies into it itself (crashes there, like glibc).
  - `devices`: `/dev/null` (`O_WRONLY`) takes writes of 5, 1000 (more than a chunk) and 0
    bytes, reads `EBADF`; `O_RDONLY` reads 0 twice without touching the buffer, writes `EBADF`.
    `/dev/zero` (`O_RDWR`): 500 bytes read in a loop are zeros, byte 500 untouched, writes
    swallowed. `stat`: `S_ISCHR`, `0666`, `major`/`minor`/`makedev` 1:3 and 1:5, devfs's
    `st_dev`, size 0, `fstat` byte-identical. `/dev` lists the dots (`DT_DIR`) and `null`/`zero`
    (`DT_CHR`, `d_ino` = `st_ino`; the disks, `DT_BLK`, are the `disks` test's). `ESPIPE` (`lseek`), `ENOTDIR` (`getdents`, `opendir`,
    `O_DIRECTORY`, `/dev/null/`, `/dev/null/x`), `ENOENT` (`/dev/nope`), `EROFS` outside `/dev`;
    fd 3 free at the end. `/dev` lists `console` as `DT_CHR`; `/dev/console` opened by name
    takes a 0-byte write, is a tty and is fd 0's node. `isatty` is checked only there until the
    terminal check (step (d)).
  - `ioctl`: `TIOCGWINSZ` on fd 0 into a `0xA5`-filled struct gives 25×80 with the pixels
    zeroed and `errno` unchanged, the same through `/dev/console` by name; `ENOTTY` for an
    unknown request and on an initrd file, a directory, `/dev/null`, `/dev/zero`; `EFAULT`
    (bad pointer), `EINVAL` (a request claiming 1000 bytes), `EBADF` (99, -1).
  - `disks`: the first `DT_BLK` entry in `/dev` (`hda` on QEMU, `hdb` on Bochs; its `d_ino` =
    `st_ino`). `stat`: `S_ISBLK`, `0660`, major 3 or 22, minor 0 or 64, size and blocks 0,
    devfs's `st_dev`, `fstat` of the open disk byte-identical. `lseek(SEEK_END)` = 4 MiB; the
    ext2 magic at byte 1080 and the label at 1144; 1 KiB at 512 equals 300 bytes and then the
    rest read on from where the first read stopped (offset 1536 after it); 100 bytes at 10 before
    the end give 10 (canary kept), then 0, and 0 after seeking past the end. `read` moves at most
    256 bytes per call, so the test loops (`read_full`). Writes (sectors 7000…, free; all
    restored): one `write` of 5000 bytes from 100 bytes into sector 7000 across a cache block,
    the position right after it, read back through the same and a second fd; an `O_WRONLY` fd
    writes, `read` on it `EBADF`; 16 bytes at 10 before the end write 10, at and past the end
    `ENOSPC`, 0 bytes 0. `EINVAL` (mode 3), `EISDIR` (`/dev` for writing), `ENOTDIR`
    (`O_DIRECTORY`, `getdents`), `EBADF` (write), `ENOTTY` (`TIOCGWINSZ`), `isatty` 0; fd 3 free at
    the end.
  - `tty`: `isatty` 0 + `ENOTTY` on `/dev/null` and `/dev/zero`, 1 on `/dev/console`; and
    `isatty` 1 on fds 0–2 with `errno` untouched, 0 + `ENOTTY` on a file and a
    directory, 0 + `EBADF` on closed, 99 and -1. Then, before stdout's first write, fd 1 is
    replaced by a read-only file: `printf("x\n")` returns 2 with no stream error (fully
    buffered), and `fflush` fails with `EOF`/`EBADF`/`ferror` (line buffering would fail at the
    `printf`). Afterwards stdout is that file, so that part reports on stderr.
  - `printf`: `vsnprintf` through an `EXPECT(expected, format, ...)` helper that compares output
    and returned length and prints what it got. Every conversion, flag and length (`hh`/`h`
    narrowing, `%ld` above 2^32, `INT64_MIN`, 22-digit octal), `*` width/precision (negative ones),
    `%.3s` on an unterminated array, unknown conversions and a trailing `%`; the C99 contract with
    `#` canaries: cut mid-text/padding/number, exact fit, one short, size 1, size 0 with NULL,
    `%300d` returning 300.
- **VFS self-test** (`selftest/vfs.c`, before the ELF one): types of `/`, `/usr`, `/usr/tests`
  (also with a trailing `/`), `/sbin/init`, `/bin/sh`; `//usr///tests/hello`,
  `/./usr/./tests/./hello`, `/usr/tests/../tests/hello`, `/bin/../usr/tests/hello` give the same
  node pointer, `/..` and `/usr/..` the root; `ENOENT` (`/nope`, `/usr/nope/hello`, relative,
  `""`, `hell`, `hello.elf`), `ENOTDIR` (`hello/`, `/sbin/init/x`, `/sbin/init/.`), a 255-char
  component is `ENOENT`, 256 `ENAMETOOLONG`, a failed lookup never sets the node;
  `vfs_read_file` size equals the node's and starts with an ELF header; `vfs_read` at 0 and 100
  matches it, across the end returns the rest, at or past the end 0; `EISDIR` from `vfs_read` and
  `vfs_read_file` on a directory. `readdir`: the root lists exactly `.`, `..`, `bin`, `sbin`,
  `usr` (dots = the root), `/usr`'s `..` is the root, `hello` once in `/usr/tests` with lookup's
  inode number, `entry.length` set (poisoned before), past the end 0, a file `-ENOTDIR`.
  `vfs_file_getdents` on `/` (opened with `O_DIRECTORY`; a file gives `-ENOTDIR`): 80 bytes and
  `next` 5, then 0; `sbin` into exactly 16 bytes leaves a canary after them; 15 bytes `-EINVAL`.
  `vfs_stat` (into a `0xA5`-filled struct: every field and `__pad0` written): `hello` and
  `/usr/tests` are `0755` with an mtime between 2023 and 2100, `/usr` and `/` `0755` with mtime
  0 (catches a file's values leaking onto its parents); an open `hello`'s `file->node` is its
  lookup's node; `/dev/console` is `S_IFCHR | 0600`, 5:1, on devfs, and `console_file()` is that
  node opened read-write with the console's ops. `/dev` lists its dots, `null`, `zero`,
  `console`, then only `VFS_BLOCKDEV` entries (the disks) until the end.
  The root's and `hello`'s reference counts are unchanged after all of it (a missing `put` in
  lookup fails here).
- **ELF self-test** (`selftest/elf.c`): `exec_load` of `/usr/tests/hello` (read with
  `vfs_read_file`, freed at the end) must succeed with the header's entry;
  ten broken copies (magic, class, machine, `ET_DYN`, truncated, program headers out of bounds,
  `p_filesz > p_memsz`, segment in the kernel half, segment wrapping past 2^64, entry outside every
  segment) must return `-ENOEXEC` without setting the entry. The valid load passes
  `argv = {"elf", "selftest"}`, `envp = {"A=1"}` and checks the initial stack from the kernel side
  (16-aligned, `argc`, the NULLs, `AT_NULL`, the strings behind `argv[1]` and `envp[0]`).

### Block devices (`driver/block.c`, `driver/block_cache.c`, `platform/pc/ata.c`)

```text
read(fd) ─▶ vfs_file_ops ─▶ vfs_read(node, offset) ─▶ devfs_block_read ─▶ block_read_bytes(dev, buf, count, offset)
                                                     per 4 KiB block: block_cache_get ─ memcpy slice ─ block_cache_put ─┘
ext2 (step 4) ─▶ block_cache_get ─ read buf->data in place ─ block_cache_put
block cache, on a miss ─▶ block_read(dev, sector, count, buf) ─range check─▶ dev->ops->read
                                                       ATA PIO: ports, polling, inw ×256 per sector
```

- **Block layer** (`include/driver/block.h`): `block_device_t` = `name[16]` (`hda`…),
  `sector_count`, `rdev` (`MIHOS_MKDEV`, chosen by the driver), `const block_ops_t *ops`
  (`read(dev, sector, count, buffer)` and `write(dev, sector, count, const buffer)`: kernel
  buffers of `count * BLOCK_SECTOR_SIZE` (512) bytes, 0 or -errno; `flush(dev)`; `write` NULL:
  read-only, `flush` NULL: no write cache), `private` (the driver's data).
  `block_register` (static table of `BLOCK_DEVICES_MAX` 8; duplicate → `-EEXIST`, full →
  `-ENOMEM`; boot time, no lock; the device lives forever), `block_find(name)`, `block_read`:
  `count` 0 → 0, `sector >= sector_count || count > sector_count - sector` → `-EINVAL` (cannot
  wrap), then the driver. The range check lives only here. **Only the cache calls
  `block_read`** (and the block self-test, to compare): a read around it would miss cached
  data and, once there are writes, see stale data. `block_write`: the same checks, then no
  `write` op → `-EROFS` (after the range check: a bad range is `-EINVAL` on any disk), then the
  driver; also only for the cache (a write around it leaves a cached copy stale).
  `block_flush(dev)`: no `flush` op → 0, else the driver's result.
- **Byte reads** (`block_read_bytes(dev, buffer, count, offset)`): `offset` at or past the end →
  0, `count` cut to `size - offset` (a subtraction, so nothing wraps); then per step: `index =
  position / 4096`, `within = position % 4096`, `chunk = MIN(count - done, 4096 - within)`,
  `block_cache_get`, `memcpy` of the slice, `block_cache_put` right away (copy-out: the caller has its
  own bytes). A `block_cache_get` error is returned as is, never a short count (it would hide the
  error). Returns the bytes read.
- **Byte writes** (`block_write_bytes(dev, buffer, count, offset)`): `count` 0 → 0 (also at
  the end), `offset` at or past the end → `-ENOSPC`, `count` cut to `size - offset` (a short
  write); per 4 KiB block `block_cache_get` (which still fills it from the disk, even when the
  slice is the whole block), `begin_write`, `memcpy` in, `end_write`, `put`. The bytes reach the
  disk at eviction or the next sync. Errors as `block_read_bytes`: returned as is.
- **Block cache** (`include/driver/block_cache.h`, `driver/block_cache.c`): `BLOCK_CACHE_BUFFERS` 64 buffers of
  `BLOCK_CACHE_BLOCK_SIZE` = `PAGE_SIZE` (8 sectors; 1/2/4 KiB ext2 blocks are slices of one),
  256 KiB, allocated by `block_cache_init` (`kmalloc`, page aligned; failure panics).
  `block_cache_buf_t` = key `dev` + `index`, `refs`, `lru_link` (`list_t`), `hash_link` (`hlist_node_t`),
  `data_lock` (mutex), `valid`, `dirty`, `data`. **Leases:** `block_cache_get(dev, index, &buf)` → a referenced,
  filled buffer, or `-EINVAL` (index past `ceil(sector_count / 8)`), `-ENOBUFS` (every buffer
  leased), the disk's error; `block_cache_put(buf)` (a put at 0 references panics). A buffer with
  references is never reused, so a holder may read `data` until its put. **Claim**
  (`block_cache_claim`, under `block_cache_lock`, a spinlock never held during I/O): look the key up in
  `block_cache_hash` (16 buckets, `hash_64((uintptr_t)dev ^ index, 4)`, keys compared in the
  bucket walk) → `refs++`, a hit; else walk the LRU from the back (least recent) for the first
  `refs == 0` buffer, `hlist_remove` it from its old bucket, set the key, `valid = false` (safe
  without `data_lock`: at 0 references nobody is inside it), `refs = 1`, add it to the new bucket, a
  miss; none free → `-ENOBUFS` (no sleeping: a leak shows as an error, not a hang). **Fill**
  (after the lock, under `data_lock`): if `!valid`, read `block_cache_sectors` (`min(8, sector_count - index * 8)`)
  sectors (the disk's partial last block zero-filled past the end), `valid = ret == 0`; a
  failed read puts the buffer and returns the error (the next get retries). A second reader of
  the same block hits, waits on `data_lock`, finds it valid: one disk read for both. **Put:** under
  the lock, `refs--`; at 0 `list_move_after(&block_cache_lru, ...)` (the front: most recent).
  `block_cache_stats(&hits, &misses)`: claims that found the key and claims that took a buffer (a
  retry after a failed read counts as a hit).
- **Writes and write-back:** `data_lock` guards `data`, `valid` and `dirty`; it is held for a
  fill, a write-back, and by a writer between `block_cache_begin_write(buf)` (a held buffer) and
  `block_cache_end_write(buf)` (sets `dirty`). Only a holder sets `dirty`, only a write-back
  clears it, both under `data_lock`, so at 0 references it cannot change and claim reads it
  under the spinlock. One `data_lock` at a time (two could deadlock). Readers take no lock:
  consistency of concurrent reads and writes is the file system's job. **Eviction:** when the
  oldest free buffer is dirty, claim sets `refs = 1` under its old key (a get of that block
  still hits it and waits: the only correct copy is never lost) and returns
  `BLOCK_CACHE_WRITE_BACK`; `block_cache_get` runs `block_cache_write_back` (under `data_lock`:
  dirty again checked, all of `block_cache_sectors` written, `dirty = false` on success),
  `block_cache_unpin` (`refs--` without moving it: it stays at the back, the first the retry
  takes), and claims again from scratch (its own block may have been cached meanwhile, or the
  victim dirtied again); a failed write-back fails the get, the buffer stays dirty.
  **`block_cache_sync()`:** every buffer with a `dev` is pinned (`refs++`, so claim skips it;
  held ones too, since only the write-back reads `dirty` reliably), written back, unpinned;
  then `block_flush` on every registered disk; goes on after errors, returns the first.
  `system_halt` calls it before `irq_disable` (it takes mutexes) and logs a failure; `panic`
  does not sync.
- **Block devices in `/dev`:** the disks are found in `platform_init`, before `kmalloc` and
  devfs exist, so `block_devfs_init()` (in `kmain`, after `console_init`: `/dev` lists `null`,
  `zero`, `console`, then the disks) walks the table and calls `devfs_register_block(name,
  0660, dev)` for each (failure panics). devfs: `devfs_add(name, type, mode, rdev, ops, &node)`
  holds what both registrations share (name checks, `kzalloc`'d `devfs_node_t`, inode number,
  list insert; boot time only, so inserting before the caller fills in the rest is safe);
  `devfs_register` adds `device_ops`, `devfs_register_block` sets `block` (in `devfs_node_t`)
  and `node.size = sector_count * 512`. The node's ops are `read` (`devfs_block_read` →
  `block_read_bytes`) and `write` (`devfs_block_write` → `block_write_bytes`). Opened like a
  file (`vfs_file_ops`, with the access asked for), so `lseek` and `SEEK_END` work; `ioctl` → `ENOTTY`; `stat` → `S_IFBLK | 0660`,
  size 0, `st_rdev`. Numbers: Linux's IDE ones, major 3 (`IDE0_MAJOR`, primary) or 22
  (`IDE1_MAJOR`, secondary), minor `slave * 64` (64 minors per drive for its partitions).
- **ATA PIO** (`platform/pc/ata.c`, PC ports, so `platform/`; `ata.h` declares only
  `ata_init`): `ata_init()` at the end of `platform_init` (before `mm_init`: four static
  `ata_drive_t` slots `{io, control, slave, block}`, no `kmalloc`). Writes nIEN (`0x02`) to both
  control ports (polling; no IRQ 14/15 handler), then per slot (primary `0x1F0`/`0x3F6`,
  secondary `0x170`/`0x376`, master/slave) `ata_identify`: select (`0xA0 | slave << 4`),
  delay, zero count/LBA, `IDENTIFY` (`0xEC`); status 0 or `0xFF` → no drive; wait for not-BSY;
  LBA mid/high non-zero → ATAPI (the boot CD), skipped; then 256 words, words 60–61 = the 28-bit
  LBA sector count (0 → no LBA, skipped). A disk is registered as `hda`…`hdd` by position and
  logged (`ata: hda, 4 MiB`).
- **Helpers:** `ata_delay` reads the alternate status (the control port, no side effects) 4
  times, about 400 ns, after every drive select and after a command; `ata_wait_ready` spins
  while BSY (limit 1,000,000 polls → `-EIO`) and returns the status, ignoring ERR (IDENTIFY on
  a CD aborts on purpose); `ata_wait_data` = ready, then ERR/DF → `-EIO`, no DRQ → `-EIO`;
  `ata_read_data` waits for data and copies `size / 2` words with `inw`; `ata_write_data` waits
  for data (DRQ: the drive takes the sector) and sends `size / 2` words with `outw` (no `rep
  outsw`: real drives may need the gaps).
- **`ata_command(drive, lba, n, command)`:** `ata_wait_ready` (not `_data`: an idle drive has
  no DRQ), select `0xE0 | slave << 4 | LBA[24..27]`, delay, count (`n & 0xFF`, 0 = 256), LBA
  bytes 0–23, the command, delay.
- **`ata_read`:** past 2^28 sectors → `-EINVAL` (LBA48 is debt). Per command of `n` ≤ 256
  sectors: `ata_command(READ SECTORS 0x20)`, then per sector `ata_read_data` of 512 bytes, the
  `uint16_t *` advancing 256 words. The driver runs on the caller's thread (no thread of its
  own: a disk only answers when asked).
- **`ata_write`:** the same loop with `WRITE SECTORS` (`0x30`) and `ata_write_data`; after each
  command's last sector the drive is BSY while it stores the data, so `ata_wait_ready` again
  and ERR/DF → `-EIO` (the end of the data phase is not the end of the write; without the
  check a failed write looks fine). **`ata_flush`:** wait ready, select `0xE0 | slave << 4` (no
  address), `CACHE FLUSH` (`0xE7`), wait ready, ERR/DF → `-EIO`; no data phase.
- **Test disk** (`Makefile`): `disk.img`, 4 MiB ext2 with 1 KiB blocks, label `mihos-test`,
  root-owned, `mke2fs -q -F -t ext2 -b 1024 -L mihos-test -E root_owner=0:0 -d build/disk`,
  rebuilt when a file in `disk/` or the Makefile changes; `make clean` deletes it and `build/`.
  `build/disk` is `disk/` (`hello.txt`, `empty`, `docs/README`, `docs/deep/nested/file.txt`;
  `.gitattributes` keeps them byte-exact, `-text`) plus generated files: `big.txt` (300 KiB of
  `"%07d\n"` lines holding their own number: direct, single and double indirect blocks),
  `sparse` (`"start"`, a hole to 16384, `"end"`: holes in `i_block[1..11]` and inside the
  single-indirect block), a 255-character name, `link` (a symlink), and `grow/` (files only
  the growth tests touch, since nothing can shrink a file back yet: `small` 5 bytes, `empty`,
  `holes` with `sparse`'s layout, `long` with `big.txt`'s content, `user` for the `ext2`
  test). What `mke2fs` makes of
  it: revision 1, 256-byte inodes, incompat `FILETYPE` only, compat `0x38`, ro_compat
  `0x3`, one group, root directory in block 276. QEMU (`run-tests.sh`, and `run.bat` for
  interactive runs): IDE primary master (`hda`), `-snapshot`. Bochs (local `.bochsrc`):
  `ata0-slave: type=disk, path=disk.img, mode=flat` (`hdb`: the CD is `ata0-master` there).
- **Block self-test** (`selftest/block.c`, after the VFS one): the first of `hda`…`hdd` (none
  → panic), 8192 sectors; sector 2 is the ext2 superblock (magic `0xEF53` at 56, block-size
  exponent 0 at 24, label at 120); a 2-sector read and a 300-sector read (two commands) equal
  single reads, also at sectors 255/256/257; 0 sectors does nothing; the last sector reads;
  past the end, from the last sector for 2, `UINT64_MAX` and `UINT32_MAX` → `-EINVAL`; a
  second `block_register` → `-EEXIST`. Writes (`block_test_writes`, sectors 7000…7299: free
  ext2 blocks 3500…3649 nothing else reads, so no cached copy goes stale): 300 sectors (two
  commands), each with its own pattern (`sector * 31 + byte`), read back sector by sector; one
  sector at 7100 changes only itself; 0 sectors → 0, the same past-the-end cases → `-EINVAL`;
  `block_flush` 0; an unregistered device without `write` → `-EROFS`, a bad range on it still
  `-EINVAL`, its flush 0; the original sectors written back and compared (a persistent disk
  stays unchanged). Then the cache (`block_test_cache`, on blocks nothing
  else reads): two gets of block 1 share one buffer (`refs` 2; one miss, then one hit), its
  data equals its 8 sectors, after both puts it is still cached; with block 2 held, 64 new
  blocks (600…663) evict exactly the oldest free one: the held buffer keeps its key and data,
  663 hits, 600 misses again; 64 leases (700…763) make a new block `-ENOBUFS` (`out` untouched)
  while a cached one still hits, every `refs` 0 afterwards; block 1024 and `UINT64_MAX`
  `-EINVAL`, the last block fine. Then `/dev/<disk>`: `VFS_BLOCKDEV` with the driver's
  `rdev` (an IDE major) and the disk's size; `vfs_stat` `S_IFBLK | 0660`, size and blocks 0;
  `O_RDONLY`/`O_WRONLY`/`O_RDWR` open with the same node and exactly `FILE_READ`/`FILE_WRITE`/both,
  mode 3 `-EINVAL`; seven `vfs_read`s
  equal a 300-sector `block_read` with a canary after each (a whole sector, 2 bytes inside one,
  across sectors 1–2, head + 2 sectors + tail, 270 sectors from 512, everything from byte 3,
  around the 256-sector command boundary); the magic at byte 1080; 100 bytes at 10 before the
  end give the last 10 (canary kept); at the end, past it, at `UINT64_MAX` and for 0 bytes 0;
  64 KiB at block 200 read twice: 16 misses, then 16 hits and none; the node's references
  unchanged. Write-back (`block_test_write_back`, after the cache section, on cache blocks 875
  and 876 = the write section's sectors): a written buffer is `dirty` and the disk keeps the old
  bytes until `block_cache_sync`, then holds the new ones; a dirty block evicted by 64 new
  blocks (920…983) is on the disk, and its next get misses and reads it back; a held dirty
  buffer is written by a sync and keeps `refs` 1; after a sync 64 uncached blocks (300…363) are
  leased at once (a reference leaked by the sync would give `-ENOBUFS`); both blocks restored
  through the cache.
- **Hash table self-test** (`selftest/hashtable.c`, right after the guard one): one bucket by
  hand (every `next`/`pprev` after inserts at the front and removes of the middle, first and only
  node; `hlist_linked`; an emptied bucket takes a node again); 100 keys in 4 buckets found
  exactly once by comparing keys, missing keys not found, the odd keys left after removing the
  even ones, every bucket empty at the end; `hash_64` below `2^bits` for 1–16 bits, the plain
  product at 64, 1024 consecutive keys 48–80 per bucket of 16; `list_move_after` from the back
  and the middle (both link directions), and of the first item (no change).

### ext2 (`fs/ext2/`, `include/fs/ext2.h`)

```text
fs/ext2/
├── internal.h     private to the folder: the volume and node structs, what one file calls in another
├── disk_format.h  the on-disk structures (ext2_super_t, ext2_group_t, ext2_inode_t, ext2_dirent_t)
├── disk.c         disk access: ext2_block_get/write/zero, ext2_disk_write/zero
├── allocation.c   block and inode bitmaps, free counts: block_alloc/free, inode_alloc/init/free
├── inode.c        the ops tables, inode_byte, inode_store, links_store, node_get
├── file_blocks.c  slot, file_block, truncate_branch
├── directory.c    record checks, dir_walk, dir_add/remove, readdir, lookup
├── file_data.c    read, write, truncate
├── namespace.c    create, unlink, mkdir, rmdir, release (frees an unlinked node at its last put)
└── mount.c        superblock checks, group table, s_state, halt hook, mount, init
```

Only `include/fs/ext2.h` (`ext2_mount`, `ext2_init`) is seen outside the
folder. A function another file of the folder calls is not `static` (it keeps the `ext2_`
prefix) and is declared in `internal.h`, grouped by the file defining it; the rest is
`static`. Each file is in the bottom-up order of the single file it came from (every
function after what it calls).

```text
/ (initrd) ─ mnt ── mounted ──▶ ext2 root (inode 2) ─ hello.txt (inode 20)
                                                    └ docs (14) ─ deep ─ nested ─ file.txt
read(/mnt/big.txt) ─▶ ext2_read ─ per ext2 block: ext2_file_block ─▶ ext2_block_get ─▶ block cache lease ─ memcpy
lookup(dir, name) ─▶ ext2_dir_walk(visit) ─ records in leased blocks ─▶ ext2_node_get(ino) ─▶ node cache
```

- **On-disk format** (`fs/ext2/disk_format.h`, private to the folder): `ext2_super_t` (1024 bytes at byte
  1024 of the disk), `ext2_group_t` (32), `ext2_inode_t` (the first 128 bytes of an inode),
  `ext2_dirent_t` (8-byte header + name), with `_Static_assert`s on sizes and offsets; read in
  place from cache buffers, little-endian like x86 (a big-endian arch needs conversions).
  `ext2_super_t` names `s_min_extra_isize`/`s_want_extra_isize` (348, 350) between two unused
  ranges; `EXT2_INODE_EXTRA_ISIZE` (128: a large inode's `i_extra_isize`), `EXT2_INDEX_FL`
  (`0x1000`, an htree directory), `EXT2_DIRENT_SIZE(name_len)` (8 + the name, rounded up to 4).
- **Mount** (`ext2_mount(disk, &root)`): copies the superblock from cache block 0, then
  `ext2_check_superblock`: no magic, revision ≠ 1, an incompat feature other than `FILETYPE` or no
  `FILETYPE`, blocks over 4 KiB (they would straddle cache buffers), an inode size not a power of
  two in 128..block size → `-EINVAL` ("not ours"); zero blocks or inodes per group, the first
  data block past the end, more blocks than the disk, groups too few for `s_inodes_count` →
  `-EIO` (inconsistent). Compat features are ignored; a `ro_compat` one other than
  `SPARSE_SUPER`/`LARGE_FILE` (`EXT2_FEATURE_RO_COMPAT_KNOWN`), or a disk without a `write`
  op (nothing could be written back), makes the volume `read_only` (Linux's three-tier rule).
  A group with more blocks or inodes per group than its one-block bitmap has bits → `-EIO`;
  `s_first_ino` at or below the root (2) or past `s_inodes_count` → `-EIO`. `want_extra_isize`
  for large inodes: `s_want_extra_isize`, at least `s_min_extra_isize`, 32 when both are 0
  (an old `mke2fs`; Linux's default); 0 with 128-byte inodes.
  `max_file_size`: what 12 + p + p² + p³ blocks hold (p = block size / 4; 16 GiB with 1 KiB
  blocks), under 1 TiB (`i_blocks` is 32-bit 512-byte units), under 2 GiB without
  `LARGE_FILE`. `ext2_read_groups` copies the descriptor table (from block
  `s_first_data_block + 1`, over as many blocks as needed) into a `kmalloc`'d array and checks
  every group's inode table lies inside the file system, so `ext2_node_get` trusts it. Then
  `mutex_init` of both locks, the free counts from the superblock, `vfs_dev_alloc`, and the
  root (`ext2_node_get(2)`; not a directory → `-EIO`).
  `ext2_volume_t` = the disk, `dev`, block size, block/inode counts, inodes per group, inode size,
  groups, `first_data_block`, `blocks_per_group`, `first_ino`, `want_extra_isize`, `dtime`
  (freed inodes' `i_dtime`: `s_wtime`, at least `s_inodes_count`),
  `read_only`, `max_file_size`, the group
  table, `free_blocks`/`free_inodes` and `allocation_lock` (the bitmaps, the group table's
  counters and those two), `lock` (the node cache) and the node hash (`nodes[64]`, `kzalloc`'d:
  empty). The files follow the old single file's sections (see the tree above); the ops
  tables are in `inode.c`, with `ext2_node_get`, their only user, and every op is declared in
  `internal.h`.
- **Disk access** (`ext2_block_get(volume, block, &lease, &data)`): the only way to the disk; block 0
  or past the end → `-EIO` (a corrupt pointer); otherwise `block_cache_get` of the cache block
  holding it and `data` = its first byte there (a block never straddles two cache buffers).
  Every caller puts the lease right after use; at most one is held at a time.
  `ext2_block_write(volume, block, within, data, count)`: the same lease, `begin_write`, the
  slice copied in, `end_write`, put (the pair in one place, so no caller holds a buffer's write
  across another lease). `ext2_disk_write(volume, byte, data, count)`: any byte range of the
  file system through `block_write_bytes` (a short write → `-EIO`), for metadata outside the
  data blocks or smaller than one (the superblock is in block 0 with 4 KiB blocks, which
  `ext2_block_get` refuses; descriptors; inode fields). `ext2_block_zero`: a range of a block
  zeroed in its buffer (no 4 KiB zero buffer on the stack). `ext2_disk_zero(volume, byte,
  count)`: zeros anywhere, written from a static 128-byte zero array as many times as needed
  (an inode slot is up to a block).
- **Block allocation** (under `allocation_lock`): `ext2_bitmap_find(bitmap, from, bits)` (the
  first clear bit in `[from, bits)`, bit 0 = the lowest bit of byte 0, `-ENOSPC` if none),
  `ext2_bitmap_set(bitmap, bit, used)` (the byte read, the lease put, the byte written back;
  a bit already as wanted → `-EIO`), `ext2_group_store` (the descriptor from the in-memory
  table into the primary table at block `first_data_block + 1`), `ext2_super_store_counts`
  (`s_free_blocks_count` and `s_free_inodes_count`, adjacent, into the primary superblock).
  `ext2_block_alloc(volume, goal, &block)`: no free blocks → `-ENOSPC`; a goal outside the
  data area → the first data block; `groups + 1` rounds from the goal's group (the goal's
  group from the goal, the others from 0, the goal's group again from 0: a block before the
  goal is found last instead of never), groups with no free blocks skipped, the last group's
  real size; found → the bit set, both counters −1, descriptor and counts written, the block
  zeroed; nothing found despite the counters → `-EIO`. Block = `first_data_block` + group ×
  `blocks_per_group` + bit. `ext2_block_free(volume, block)`: outside the data area or already
  free → `-EIO`; the bit cleared, both counters +1, both written. Only the primary superblock
  and descriptors (the `sparse_super` backups go stale, as on Linux).
- **Inodes on the disk:** `ext2_inode_byte(volume, ino)` = the group's `bg_inode_table` ×
  block size + slot × `inode_size`, the one formula for reads (`ext2_node_get` splits it into
  block and place) and writes. `ext2_inode_store(n)` (caller holds `n->lock`) writes
  `i_size`, `i_blocks`, `i_block` and, for regular files only, `i_size_high` (a directory's is
  `i_dir_acl`), field by field through `ext2_disk_write`: nothing is read, and fields it does
  not own (links, times) are never rewritten (`ext2_links_store` writes `i_links_count`).
- **Nodes** (`ext2_node_t` = `vfs_node_t` (as `base`) + `volume`, `ino`, a copy of `i_block[15]`,
  `blocks` (`i_blocks`), `flags` (`i_flags`), `free_on_release` (its last name was
  removed), `lock` (a mutex: `block[]`, `blocks`, `base.size` and `nlink`, and a directory's
  records: held by `ext2_read`, `ext2_write`, `ext2_truncate`, `ext2_readdir`,
  `ext2_lookup`'s walk, `ext2_create` and `ext2_unlink`; a directory's lock before its
  children's; lock order
  `n->lock` → `volume->lock` → the cache's), `cache_link`; ops `ext2_ops`, or
  `ext2_read_only_ops` (no `write`: the VFS refuses write opens with `-EROFS`) on a `read_only`
  volume):
  `ext2_node_get(volume, ino, parent, &node)`, under `volume->lock` (a mutex: it may read the disk; taken
  before the cache's locks, never inside them): ino 0 or past `s_inodes_count` → `-EIO`; a cached
  node is returned only through `vfs_node_tryget` (at 0 it is dying: unhashed, and a fresh one is
  read); otherwise the inode is found at group `(ino-1) / inodes_per_group`, slot `(ino-1) %
  inodes_per_group`, `inode_size` bytes each, from the group's `bg_inode_table`; only regular
  files and directories become nodes (`-ENOENT` otherwise). The node gets `mode & 07777`,
  `mtime`, `nlink` (`i_links_count`), `size` (`i_size`, plus `i_size_high << 32` for files:
  large_file), `parent` (the directory it was found in, with a reference, so `..` stays valid;
  the root is its own parent). **Release:** unhash under the lock if still linked, `kfree`, then
  put the parent after the lock (that may release it in turn: a chain of releases up the tree).
- **Inode allocation** (bitmaps and counts under `allocation_lock`):
  `ext2_inode_alloc(volume, parent, directory, &ino)`: no free inodes → `-ENOSPC`; the parent's
  group first, then each other group once (`groups` rounds: an inode has a goal group, not a
  goal bit, so every group is searched from its start), groups with no free inodes skipped;
  group 0 from bit `first_ino - 1` (a reserved inode is never given out, even with its bit
  clear), the last group's real size; found → the bit set, both counters −1 (and
  `bg_used_dirs_count` +1 for a directory), descriptor and counts written; ino = group ×
  `inodes_per_group` + bit + 1 (inodes count from 1); nothing found → `-EIO`. The slot is not
  touched: `ext2_inode_init(volume, ino, mode, links, time)` zeroes all `inode_size` bytes (a
  reused slot keeps its last owner's block map, `i_dtime` and in-inode extended attributes,
  `ext_attr` being on), writes an `ext2_inode_t` with the mode, the links and the three times,
  and `want_extra_isize` at byte 128. `ext2_inode_free(volume, ino, directory)` (called by
  `ext2_release`): below `first_ino` or past the count → `-EIO`; `i_dtime` = `volume->dtime`
  first, outside the lock (no clock, so the superblock's last write time, `s_wtime`;
  non-zero means deleted to `e2fsck`, 0 an orphan; and one below `s_inodes_count` is read
  as the next inode of the orphan list, a "corrupted orphan list", which is why it is never
  1), and `i_links_count` 0 (e2fsck calls an inode with links in use whatever its bit says;
  a create or mkdir failing after `ext2_inode_init` gives back a slot still saying 1 or 2:
  the full-disk test found it), then the bit cleared (already
  clear → `-EIO`), the counters +1 (`bg_used_dirs_count` −1), both written. On the test disk:
  `first_ino` 11, the first free inode 29, `want_extra_isize` 32.
- **`ext2_slot(volume, n, table, index, goal, create, &block)`:** one block pointer: `n->block[index]`
  (table 0) or entry `index` of table block `table`. With `create` and a 0 there:
  `ext2_block_alloc(*goal)`, linked at once (in `n->block[]`, or written into the table; if that
  write fails the block is freed again), `n->blocks += block_size / 512` (tables count, as in
  `i_blocks`). `*goal` moves past every block seen (NULL: not tracked; readers).
- **`ext2_file_block(volume, n, logical, goal, create, &block)`:** 12 direct pointers, then
  `i_block[12..14]` with 1, 2, 3 levels of tables of `block_size / 4` entries (256 with 1 KiB
  blocks: +256 KiB, +64 MiB, +16 GiB); walks down one slot per level like a page table; without
  `create` 0 anywhere is a hole, with it every missing table and the data block are created on
  the way down; past the triple range `-EFBIG`.
- **Directories:** `ext2_dir_walk(dir, visit, context)` (the caller holds `dir->lock`) runs
  `visit(entry, context)` (a function pointer plus a context struct: a closure by hand) for
  every used entry, block by block, while the block is leased; 0 goes on, 1 stops (the walk
  returns 1), -errno fails. Each record is checked first (`ext2_record_bad`, shared with
  `ext2_dir_add`): 8 bytes of header left in the block, `rec_len` ≥ 8, a multiple of 4, inside the
  block, holding `8 + name_len` → else `-EIO` (never a loop or a read past the block). Inode 0
  (deleted) is skipped. `readdir` counts shown entries (regular files and directories: others
  are skipped and not counted, so indexes stay dense) up to `index`; `.` and `..` come from the
  disk. `lookup` compares `name_len` and the bytes (exact, case-sensitive), then calls `ext2_node_get`
  after the walk with no lease held; a match that is not a file or directory is `-ENOENT`.
  `ext2_dir_add(dir, name, length, ino, file_type)` (lock held, the name checked absent): the
  first record whose `rec_len` minus what it uses (`EXT2_DIRENT_SIZE(name_len)`, 0 for a
  deleted one) holds the new record is split: it keeps what it uses, the new record takes the
  rest, so the chain still ends at the block's end (the test disk's root: `sparse` at 396 has
  `rec_len` 628 for 16 used bytes); written in place in the leased buffer between
  `begin_write`/`end_write`. No room: a new block past the end (`ext2_file_block` with create:
  zeroed, counted, a table if needed) holding one record that covers it, the size +1 block,
  the inode stored. **Create** (`ext2_create(dir, name, length, mode, &child)`, the `create`
  node op, writable volumes only): under the directory's lock; `EXT2_INDEX_FL` → `-EROFS`
  (adding a record without updating the hash index would hide it from Linux's lookups); the
  name found by a walk with `ext2_lookup_visit` (any type: `link`, a symlink, too) →
  `-EEXIST`; `ext2_inode_alloc` (the directory's group), `ext2_inode_init(EXT2_S_IFREG | mode
  & 07777, 1 link, the directory's mtime)`, then `ext2_dir_add` (`EXT2_FT_REG_FILE`): the name
  last, so no record points at an unprepared inode; a failure of either gives the inode back.
  Then `ext2_node_get` (its error returned; the file stays on the disk). The directory's
  links and times stay (a file adds no `..`; no clock). `ext2_create` must not call
  `ext2_lookup` (it takes the same, non-recursive lock). `ext2_dir_remove(dir, name,
  length)` (lock held): the record found by name is swallowed by the record before it in its
  block (its `rec_len` grows by the removed one's: the test disk's `link`, 12 → 640 if
  `sparse` went), or, first in its block, gets inode 0 (`previous` follows one record behind,
  deleted ones included); `-ENOENT`, `-EIO` for a corrupt record. A directory never shrinks.
- **Deleting** (its own section, after "File data"): `ext2_unlink(dir, name, length)` (the
  `unlink` node op, writable volumes only), under the directory's lock: `EXT2_INDEX_FL` →
  `-EROFS`; a walk with `ext2_lookup_visit` (not found, or a kind the VFS cannot show →
  `-ENOENT`: a fast symlink keeps its target in `i_block`, which a truncate would free as
  block numbers); the node read *before* anything changes (`ext2_node_get`: a failure later
  never leaves a nameless file with a link); a directory → `-EISDIR` (rmdir's); 0 links
  already → `-EIO`; `ext2_dir_remove`; then under the file's lock `nlink` −1 and
  `i_links_count` written (a `uint16_t`, 2 bytes), at 0 `free_on_release` set (even if the
  write failed: the name is gone either way). The node is put after the directory's lock.
  **`ext2_release`** (moved here, after what it calls): with `free_on_release`,
  `ext2_truncate(node, 0)` and `ext2_inode_free` first (a failure is logged and leaks:
  `e2fsck` mends a leak), then the unhash, `kfree` and the parent's put as before. Only the
  flag frees, never `nlink == 0` alone (a corrupt record pointing at a 0-link inode must
  not free it on a lookup). Freeing before the unhash is safe: a create that reuses the
  inode meanwhile finds the dying node at 0 references and reads the new one.
- **Directories made and removed** (`namespace.c`). A directory has 2 links when empty (its
  name and its `.`); its parent one more per subdirectory (the child's `..`; the test disk's
  root: 5); `bg_used_dirs_count` counts them per group (6 on the test disk). `ext2_mkdir(dir,
  name, length, mode)`, under the parent's lock: `EXT2_INDEX_FL` → `-EROFS`, a removed parent
  (`nlink` 0) → `-ENOENT`, `EXT2_LINK_MAX` (32000) links → `-EMLINK`, the name taken →
  `-EEXIST`; `ext2_inode_alloc(…, directory = true)`, `ext2_inode_init(EXT2_S_IFDIR | mode, 2
  links, the parent's mtime)`, `ext2_node_get`; then under the new directory's lock two
  `ext2_dir_add`s on the empty node: `.` (the first block, one record covering it) and `..`
  split off it, `grow/`'s layout (`.` 12 bytes, `..` the rest); then its name in the parent
  (`EXT2_FT_DIR`) and the parent's `nlink` +1 (`ext2_links_store`, which writes
  `i_links_count` from `nlink`; unlink uses it too). A failure before the node exists frees the
  inode; after, the node is marked (`nlink` 0, `free_on_release`) and the put frees its block
  and inode. `ext2_rmdir(dir, name, length)`: the name looked up like unlink's (`-ENOENT`), the
  node read, not a directory → `-ENOTDIR`; under its own lock (parent before child, so no
  create can slip in) `ext2_dir_empty` (a walk stopping at any name but `.`/`..`) → else
  `-ENOTEMPTY`, `ext2_dir_remove`, its links to 0 (both at once, as Linux) and
  `free_on_release`; then the parent's `nlink` −1. The last put frees it (`ext2_inode_free`
  with `directory`: `bg_used_dirs_count` −1). `ext2_create` and `ext2_mkdir` refuse a parent
  with `nlink` 0 (`-ENOENT`): a directory removed while still held takes no new names.
- **Read** (`ext2_read`): clamped to the size; per ext2 block: `ext2_file_block`, a hole → `memset` 0,
  else the slice copied from the leased block; errors are returned as is.
- **Write** (`ext2_write`, under `n->lock`): 0 bytes → 0; at or past `max_file_size` → `-EFBIG`,
  cut to it; the goal starts after the block before `offset` (else at the inode's group's first
  block); per ext2 block `ext2_file_block` with create (a 0 block with success → `-EIO`), then
  `ext2_block_write`. Errors leave the loop instead of returning: the size grows to what was
  written, and if the size or `blocks` changed the inode is stored (`ext2_inode_store`), so
  blocks linked before an error are in the inode (consistent, maybe unused); returns the bytes
  written, or the error if none were. No times (no clock). `VFS_APPEND` as the offset is
  replaced by the size right after the lock is taken (`O_APPEND`).
- **Truncate** (`ext2_truncate`, under `n->lock`): past `max_file_size` `-EFBIG`; growing or
  the same size only sets the size (a hole) and stores the inode. Shrinking: the new last
  block's bytes past the new end are zeroed (`ext2_block_zero` takes a range), so they read
  as zeros if the file grows again; `keep` = the blocks that stay (`ceil(size / block size)`);
  the direct pointers from `keep` on and each tree go through `ext2_truncate_branch(volume,
  n, &slot, depth, span, from)`: a depth-0 slot is a data block (freed), a table's entries
  are walked (read through `ext2_slot` without create), each subtree from its relative cut
  (`from - start`, or 0 when entirely past it), an entry written back only when it became 0,
  and a table left with no pointers freed too (children before their table). A freed block's
  pointer is cleared and `blocks` counted down whether the free succeeded or not (a leak is
  mended by `e2fsck`, a block with two owners loses data). The size is set and the inode
  stored whatever happened; the first error is returned.
- **Boot mount** (`ext2_init`, after `block_cache_init`): for each `block_at(i)` (the block table in
  registration order) `ext2_mount`; `-EINVAL` skips quietly, other errors are logged; the first
  one is `vfs_mount`ed on `/mnt` (an empty directory of the initrd, `MOUNT_POINTS` in
  `grub/Makefile`), its reference put (the mount keeps one), and logged (`ext2: hda mounted on
  /mnt`); none: `ext2: no disk to mount`.
- **`s_state`:** `ext2_mount` of a writable volume, as its last step: a clear `EXT2_VALID_FS` or
  a set `EXT2_ERROR_FS` prints `ext2: hda was not cleanly unmounted or has errors: run e2fsck`
  (it mounts anyway, like Linux); `ext2_state_store` clears `EXT2_VALID_FS` on the disk (a
  failure puts the root and frees the volume); the volume's `halt_hook` (`ext2_halt`, which
  sets the bit again) is added only then, when nothing can fail. A crash or panic leaves the
  bit clear. Read-only volumes leave `s_state` alone. `volume->state` keeps the value.
- **ext2 self-test** (`selftest/ext2.c`, after the block one, whose exact cache counts it
  would disturb): `/mnt` is inode 2, a mount, on its own `st_dev`, `0755`, plausible mtime,
  `..` is `/`; the listing in disk order (`.`, `..`, `lost+found`, the 255-character name,
  `big.txt`, `docs`, `empty`, `grow`, `hello.txt`, `sparse`; `link` not shown) and `nlink` = 2 +
  subdirectories; readdir's inode = lookup's. Lookups: `link`, a prefix, a longer name, other
  case → `ENOENT`, 256 characters → `ENAMETOOLONG`, `..` inside the file system reaching the
  same nodes. Nodes: two lookups one node (`refs` 2, parent `/mnt`, `/mnt` +1); the last put
  frees it and lets `/mnt` go; a fresh node after; `docs` holds `/mnt`, a held `file.txt` holds
  `docs` through `nested` and `deep`, putting it releases the chain. Reads: three small files
  exactly, `empty` 0 bytes, `big.txt` whole (every line) and seven pieces across direct /
  single / double-indirect boundaries and the end equal to it with a canary, 0 at and past the
  end; `sparse`'s hole zeros; `hello.txt` opens `O_RDWR` with both flags, a directory opens and
  reads `EISDIR`. Writes (`ext2_test_write`): six overwrites of `big.txt` (one byte, across
  blocks 0–1, direct → single, one single-indirect block, single → double, inside double) read
  back with the bytes around them unchanged; the last 5 bytes exactly, 0 bytes 0 (also at the
  end), `UINT64_MAX` `-EFBIG`; `sparse`'s blocks 0 and 16 overwritten up to its hole, the hole
  still zeros; `empty` 0 bytes 0, `docs` `-EISDIR`; everything restored, no size changes (the
  `ext2` test reads these files later). Broken disks: an
  overlay block device reads the test disk through `block_read` and patches 1–4 bytes, one
  device per case (the cache is keyed by device): unchanged mounts, read-only (overlays have
  no `write`); an unknown `ro_compat` bit (`0x400`) mounts and reads, `hello.txt` has no
  `write` op and `vfs_write` gives `-EROFS`; no magic, revision 0, an
  unknown incompat feature, no `FILETYPE`, 8 KiB blocks, 200-byte inodes → `-EINVAL`; more
  blocks than the disk, no inodes per group, more inodes than the groups, an inode table at
  block 0 → `-EIO`; a regular-file root → `-EIO`; a zero / unaligned / too long `rec_len` and a
  name longer than its record in the root directory → mount works, `readdir` and `lookup` give
  `-EIO`. Mounted overlays stay allocated (no unmount). Growth (`ext2_test_grow`, on
  `grow/`; after each write a sync, then the raw inode's `i_size`/`i_blocks` and both free
  counts, the superblock's and group 0's): `small` +1100 bytes at 5 (`i_blocks` 4, 1 block);
  `holes` at 2000 (a direct hole: 8, 1) and at logical 13 (inside the table: 10, 1), the rest
  still zeros; `empty` 13 KiB at once (12 direct, the table and one more: 28, 14); 1 byte at
  the largest file's size `-EFBIG` (0 blocks); `long` 8 bytes at logical 524 (a new
  second-level table and its block: 610, 2), the gap after its old end zeros, its old last line
  intact. Truncate (`ext2_test_truncate`, the same checks, blocks coming back): `long` to 300 KiB
  (606, 2 back), to 13 KiB + 100 (30, 288 back: 254 single-indirect blocks, the double tree's
  32 and its 2 tables; the single table kept), up to 14 KiB + 50 (a hole, 0 blocks; the cut
  block's tail and the hole read as zeros), to 0 (15 back); `small` to 5 and a write at 1100
  (5..1099 read as zeros), to 5 again; `holes` to 1 KiB (4 back) and its `end` written again;
  `empty` to 0 (14 back); the free count at the end = before the growth test + `long`'s 303;
  `-EFBIG`, `-EISDIR`, `-EINVAL` for `/dev/hda`. Create (`ext2_test_create`, in `grow/`):
  `grow/new` with `O_RDWR | O_CREAT | O_EXCL` and `0640` is an empty file in inode 29 with
  one link, the parent's mtime and `dev`; on the disk both free inode counts −1, bit 28 of
  the inode bitmap set, `bg_used_dirs_count` unchanged, the raw slot: mode `0x8000 | 0640`,
  size, `i_dtime`, `i_blocks` 0, zeros elsewhere, `i_extra_isize` 32 and zeros after it;
  written and read back; `O_EXCL` again `-EEXIST`, `O_CREAT` alone opens it (inode 29, size
  7); 90 more names (`f000`…, 12-byte records) give `grow/` a second block (size 2048,
  `i_blocks` 4 on the disk), each looks up to inode 30 + i, 91 inodes and 2 blocks taken in
  all; refusals: `grow/dir/` `-EISDIR`, a missing parent `-ENOENT`, `hello.txt/new`
  `-ENOTDIR`, `/usr/new` and `/dev/new` `-EROFS`, `link` `-ENOENT` (`-EEXIST` with `O_EXCL`),
  `docs` opens read-only and is `-EISDIR` for writing, a 256-byte name `-ENAMETOOLONG`, and
  no inode taken by any of them. Unlink (`ext2_test_unlink`): `grow/new` opened, then
  unlinked: lookup `-ENOENT`, `nlink` 0 and `i_links_count` 0 on the disk, still written and
  read (13 bytes), no inode or block back while open; the close gives back its inode and
  block (both counts), bit 28 clear, `i_dtime` = the disk's `s_wtime`; a create then gets
  inode 29 again, and unlinking it closed frees it at once; the 90 `f…` names go (91 inodes
  back in all); the free counts equal those before the creates but for `grow/`'s second
  block, and `grow/` lists 7 entries again; refusals: `docs`, `docs/`, `docs/.`, `docs/..`
  and `/` `-EISDIR`, `hello.txt/` `-ENOTDIR`, a missing name and `link` `-ENOENT`,
  `/sbin/init` `-EROFS`, none changing the counts. e2fsck after a run: 28 inodes and
  609 blocks used (`grow/`'s second block stays), e2fsck clean. The `-EFBIG` write test uses
  `VFS_APPEND - 1` (`UINT64_MAX` appends). `disk_u32`'s sector is `static`: inlined into every
  caller with its own 512 bytes, it once took `ext2_test_unlink`'s frame to 15 KB of the 16 KB
  boot stack, and the calls below ran past its end (random `ENOENT`/`EIO`/`ENOTDIR`).
  Directories (`ext2_test_dirs`, in `grow/`): `grow/sub` is inode 29, a `0755` directory with 2
  links, size 1024, parent `grow/`, listing `.` (29) and `..` (20); on the disk its mode,
  links and `i_blocks` 2; `grow/` 3 links (node and disk), `bg_used_dirs_count` +1, one inode
  and one block taken; `sub/..` is `grow/`'s node; `slash/` made and removed with trailing
  slashes; `sub/inner` with a file: `ENOTEMPTY` for both until the file and `inner` go, `sub`
  back to 2 links; `sub` opened, removed: lookup `ENOENT`, `nlink` 0, `grow/` 2 links again,
  still lists `.`/`..`, its `create` and `mkdir` ops `-ENOENT`, still counted until the close,
  then every count back; refusals: `EEXIST` (`grow/small`, `docs`, `link`, `/`), `EROFS`
  (`/usr/new`; rmdir of `/usr` and of `/mnt`, whose parent is the initrd), `ENOENT`,
  `ENOTDIR`, `ENOTEMPTY` (`docs`, `docs/..`), `EINVAL` (`docs/.`), `EBUSY` (`/`). A full disk
  (`ext2_test_full`, about 3 s of the run): `grow/full` written 4 KiB per call until the disk
  runs out (a short write, then `-ENOSPC`), its size what was written, both free counts 0 on
  the disk, one more byte `-ENOSPC`; full, a create works (no block needed) but writing to it
  is `-ENOSPC`, a `mkdir` is `-ENOSPC` and keeps neither its inode nor the used-directories
  count; every chunk's number reads back, every 64th chunk byte for byte; unlinked while open
  it keeps its blocks until the close, then every block and inode is back. e2fsck after it:
  clean (it first caught freed inodes keeping their link count).
- **`ext2` test** (`user/tests/ext2`): the listing with types and the dots as inode 2, `link`
  absent, `hello.txt`'s `d_ino` = `st_ino`, `docs/deep/nested` lists `file.txt`, `/mnt/..` lists
  `usr`; `stat`: `/mnt` `0755`, inode 2, `nlink` 5, `st_dev` unlike `/` and `/dev`, `hello.txt`
  21 bytes one link, a path through `..` the same inode, `big.txt` `0644` with its size,
  `fstat` byte-identical; reads of the small files, `empty`, all of `big.txt` through 256-byte
  `read`s, `lseek` to lines 0/127/128/1535/1536/34303/34304/38399 (both sides of each block-kind
  boundary), `SEEK_END`, the last 5 bytes; `sparse`'s zeros; writes: `HELLO` over `hello.txt`
  through `O_RDWR` (position 5), read back through a second fd, 6 bytes at 15 (up to the end
  exactly), 0 bytes 0, `st_size` unchanged, restored; `big.txt` `O_WRONLY` across lines
  1535/1536 (direct → single), `read` on it `EBADF`, read back through another fd, restored;
  growth on `grow/user`: one 3000-byte `write`, `fstat` 3000, an append at `SEEK_END`, read back
  through a second fd, 3 bytes at 10000 (size 10003, the gap zeros); truncate on `grow/user`:
  `ftruncate` to 3003 (the kept bytes) and up to 5000 (zeros, the offset unmoved), `truncate`
  to 100, `O_RDONLY | O_TRUNC` ignored, `O_WRONLY | O_TRUNC` empties; `EINVAL` (a read-only
  fd, a negative length, `/dev/null`), `EISDIR`, `EROFS` (the initrd), `ENOENT`; create:
  `grow/made` with `O_CREAT | O_EXCL` and `0600`, written, `stat` (`S_IFREG`, `0600`, 19 bytes,
  one link), read back; `O_EXCL` again `EEXIST`; `creat` empties it and keeps `0600`;
  `made/` `ENOTDIR`, `newdir/` `EISDIR`, a missing parent and `link` `ENOENT`, `creat` on the
  initrd `EROFS`; unlink: `grow/made` (then `ENOENT`), a temp file unlinked right after its
  open, written, read back, `st_nlink` 0, gone after the close; `O_APPEND` through two fds,
  one seeked back to 0: `aaaabbbbcccc`, the offset 12 after; `EISDIR`, `ENOTDIR`, `ENOENT`
  (missing, `link`), `EROFS`; directories: `mkdir` of `grow/udir` `0700` (`stat`: a directory,
  2 links; `grow/` 3), listed as `.` and `..` only, a file inside, `rmdir` `ENOTEMPTY` (and
  its `strerror`), then `udir/` removed, `grow/` 2 again; `EEXIST`, `EROFS`, `ENOTDIR`,
  `EINVAL`, `EBUSY`;
  `ENOENT` (`link`, missing), `ENOTDIR` (`hello.txt/`, `opendir` of a file, `getdents` on a
  file), `EISDIR`, `EBADF` (write); fd 3 free at the end.

### Userland libc (`libc/`)

Our own libc until the start of roadmap phase D, then mlibc in user space (see `ROADMAP.md`).

- **Two libraries, one tree** (`libc/Makefile`): `libk.a` (kernel flags, `-D__is_libk`) and `libc.a`
  (`USER_CFLAGS`, `-D__is_libc`), each with its own object tree. In `subdir.mk`, `local_sources` go
  into both, `hosted_local_sources` (syscalls, stdio streams, `errno`, `exit`, `malloc`) only into
  `libc.a`.
  `libk.a` = `mem*`/`str*` (incl. `strlcpy`, `strcmp`, `strncmp`, `strchr`, `strspn`, `strcspn`,
  `strpbrk`, `strtok_r`), `vsnprintf`/`snprintf`, `abs`/`atoi`. User-only: `strtok` (hidden static
  position), `strdup` (`malloc`), `getenv` (`environ`), `strerror` (a table indexed by errno with
  designated initializers, NULL gaps and out-of-range codes → `"Unknown error"`; a new code in
  `<mihos/errno.h>` needs a line there). `<stdlib.h>` defines `EXIT_SUCCESS`/`EXIT_FAILURE`.
- **`crt0.o`** (`libc/arch/x86_64/crt0.S`) is built and installed on its own, not inside `libc.a`
  (archive members are only pulled for undefined symbols, and nothing references `_start`):
  `xor %ebp`; `argc = (%rsp)`, `argv = rsp + 8`, `envp = rsp + 16 + argc * 8` into
  `rdi`/`rsi`/`rdx`, `call __libc_start_main` (never returns). Assembly so the call gets the SysV
  `RSP + 8` alignment (the kernel starts it at a 16-aligned `rsp`).
- **Process start and exit** (`libc/stdlib/`, declarations in `internal/libc.h`):
  `__libc_start_main(argc, argv, envp)` sets `environ` (first: constructors may call `getenv`),
  runs `.init_array` in order, then `exit(main(argc, argv, envp))`. `exit` runs the
  `atexit`/`__cxa_atexit` table newest first (`__run_exit_handlers`, decrementing before each call
  so a handler calling `exit` never reruns one), then `.fini_array` backwards, then
  `__stdio_exit()`, then `_exit`. The table holds 32 `{function, arg}` entries (POSIX's minimum);
  `atexit(f)` stores `f` cast to take a `void *` (not portable C, fine on x86-64, as in musl).
  `__dso_handle` is defined in `atexit.c` (no toolchain `crtbegin.o`); never compared.
- **Constructor arrays** (`user.ld`): `.init_array`/`.fini_array` (and `.ctors`/`.dtors`, in
  case GCC emits the older form), prioritized entries first, between `__init_array_start/end` and
  `__fini_array_start/end` (hidden), in the data segment. C programs have empty arrays.
- **C++ runtime** (`libc/cxx/`, in `libc.a` so it leaves with our libc at phase D): `operator
  new`/`new[]` on `malloc` (size 0 → 1; out of memory prints and `abort`s: no `std::bad_alloc`),
  `delete`/`delete[]` and their sized variants on `free`, `__cxa_pure_virtual` (prints, aborts).
  No `<new>` header yet (no placement new). `stdbool.h` defines `bool` only for C; `_exit` uses
  `__attribute__((noreturn))` instead of `_Noreturn`, so every libc header works from C++.
- **Processes:** `posix_spawn` (`<spawn.h>`, `libc/spawn/`): POSIX signature, only NULL file
  actions and attributes (`ENOSYS` otherwise); returns the error number, not `-1` + `errno`
  (`__syscall`). `waitpid` (`<sys/wait.h>`, `libc/wait/`): options must be 0 (`EINVAL`), `-1` +
  `errno` otherwise. `WEXITSTATUS`/`WTERMSIG`/`WIFEXITED`/`WIFSIGNALED` decode the status.
  `environ` is in `libc/unistd/environ.c`.
- **`vsnprintf`** (`libc/stdio/vsnprintf.c`, C99 semantics, shared with the kernel): every
  character goes through `output_char`, which writes only while `length + 1 < size` and always
  counts; so the output never overruns, is terminated when `size > 0` (size 0 writes nothing, `str`
  may be NULL), and the return value is the full length (`>= size` means cut). `spec_parse` reads
  flags `- 0 + space`, width and precision (digits or `*`), lengths `hh h l ll z` (`hh`/`h` read an `int` and narrow it back to `char`/`short`, C11 7.21.6.1);
  `output_number` builds digits backwards in a local buffer, applies precision (minimum digits;
  `%.0d` of 0 prints nothing), sign and padding (`0` only without `-` and precision, zeros after
  the sign); magnitudes as `0ULL - value` (no overflow at `LLONG_MIN`). Conversions `d i u x X o p
  c s %`; `%p` is 16 zero-padded hex digits without `0x` (as the kernel always printed); `%s` NULL
  prints `(null)`; unknown conversions print as written. The helpers take a `va_list *` of a
  `va_copy`: a parameter's address is not a `va_list *` on x86-64 (array type). `snprintf` wraps
  it; `kvprintf` and `vfprintf` clamp the returned length to their buffers.
- **Private headers** (never installed, `-Iinternal -Iarch/$(ARCH)`): `internal/syscall.h`
  (`__syscall(...)` counts its arguments and picks `__syscallN`; `syscall(...)` turns `-errno` into
  `-1` + `errno` via `__syscall_ret`, Linux convention `-4095..-1`) and `arch/x86_64/syscall_arch.h`
  (inline asm per argument count: `int $0x80`, `rdi rsi rdx`, `r10 r8 r9` bound with register
  variables, only a `"memory"` clobber since the kernel preserves every register but `rax`; cast
  macros named like the functions so pointers can be passed). `internal/FILE.h` holds stdio's
  internals, `internal/malloc.h` the allocator's. Shared internals are `__`-prefixed, one per file,
  declared (and `extern` for data) in the internal header, defined once.
- `errno` is a plain global (thread-local once user threads exist). `unistd`: `write`, `read`,
  `close`, `lseek`, `_exit`, `brk`, `sbrk`, `isatty` (musl's: `ioctl(fd, TIOCGWINSZ)` succeeds;
  `ENOTTY`/`EBADF` come from the kernel), one function per file; `open` in `libc/fcntl/`
  (`<fcntl.h>`, variadic for a later `O_CREAT` mode). `O_*` and `SEEK_*` come from the uapi
  `<mihos/fcntl.h>` (also included by `<stdio.h>` and `<unistd.h>`); `off_t` is `long` (64-bit).
  `exit`: see process start and exit;
  `abort` is `_exit(134)` (128 + SIGABRT, no signals yet: no handlers or destructors run).
- **`brk`/`sbrk`:** `brk` uses `__syscall` (the kernel returns an address, not `-errno`) and sets
  `ENOMEM` when the answer differs from the request. `sbrk` asks the kernel for the break on every
  call (no cache to go stale after a direct `brk`), rejects increments that wrap, returns the old
  break or `(void *)-1`.
- **stdio** (output side done): `FILE` is opaque in `<stdio.h>`; `struct _FILE` has `fd`, `flags`
  (`F_READ/F_WRITE`, `F_EOF/F_ERR` indicators, `F_LINEBUF/F_NOBUF`), `mode` (none/read/write: what
  the one buffer holds), `buffer`/`size`/`pos`/`len`, `ungot`, `next` (list of open streams from
  `__stdio_head`). Static `BUFSIZ` buffers: `stdin` line-buffered, `stdout` line-buffered **only
  on a terminal** (`F_TTYCHECK`: its first `__fwritex` asks `isatty` once, drops `F_LINEBUF` for
  a file or pipe, and restores `errno` around the check), `stderr` unbuffered. `__fwritex` is the only code that fills the
  output buffer: pending bytes always leave first; line-buffered streams flush up to the last `'\n'`;
  unbuffered or oversized data is written directly in one go. `__write_all` loops over short writes
  (`<= 0` → `F_ERR`), `__fflush_one` writes pending output and empties the buffer (dropped on error).
  `fflush(NULL)` walks the stream list. `ferror` reads `F_ERR`. Public functions are thin wrappers: `fputc`, `fputs`,
  `puts`, `putchar`, `fwrite`, `printf`/`fprintf`/`vprintf`/`vfprintf` (formats into a `BUFSIZ`
  stack buffer, then one `__fwritex`).
- **stdio input:** in `MODE_READ` the buffer holds `[pos, len)` unread bytes from the last `read`.
  `__fillbuf` (returns 0 or `EOF`) is the only reader: not readable → `F_ERR`; `F_EOF` set → `EOF`
  without reading (sticky until `clearerr`); flushes the stream's own pending output before taking
  the buffer; flushes every line-buffered stream in `MODE_WRITE` (a prompt without `'\n'` shows
  before blocking, C11 7.21.3); `read(fd, buffer, size)` (the console returns at most one line):
  0 → `F_EOF`, < 0 → `F_ERR`. `fgetc`: `ungot` first, then the buffer, refilling when
  `pos == len`; returns 0..255 or `EOF`. `getc`/`getchar` wrap it. `ungetc`: one byte, stored as
  `unsigned char` (also the return value), clears `F_EOF`. `fgets`: loops `fgetc` until the `'\n'`
  (kept), `n - 1` characters or `EOF`; NULL for `EOF` before any character or a read error; a cut
  line continues in the next call. `feof`/`clearerr` on the flags. Ctrl+D mid-line hands over the
  typed bytes; a second one on the empty line is the end of file.
- **`stat`/`fstat`** (`<sys/stat.h>`, `libc/stat/`, translator in `internal/stat.h`): POSIX
  `struct stat` with POSIX types (`dev_t`, `mode_t`, `nlink_t`, `uid_t`, `gid_t`, `blksize_t`,
  `blkcnt_t`, `time_t` in `<sys/types.h>`, Linux x86-64 sizes) and `struct timespec` times
  (`<time.h>`), plus `#define st_mtime st_mtim.tv_sec` (and `atime`/`ctime`) for pre-2008 code;
  `S_IF*` from the uapi, `S_ISDIR`/`S_ISCHR`/`S_ISREG`, `S_IRWXU`… permission bits. Both
  functions take the kernel's `struct mihos_stat` into a local and copy it field by field
  (`__stat_from_kernel`), as mlibc's `sys_stat` will.
- **Intrusive containers** (`<sys/list.h>`, `<sys/hashtable.h>`; header-only, used by the kernel
  through libk and by malloc): mechanism only, never allocation or policy, so each user (the
  scheduler, devfs, the block cache…) builds its own structure from them, as Linux does.
  `list.h`: circular doubly linked `list_t`, `list_container`, `list_move_after` (an LRU's
  "most recent"). `hashtable.h`: Linux's hlist, `hlist_head_t` buckets (one pointer) of
  `hlist_node_t` (`next`, `pprev` = the pointer pointing at the node, so `hlist_remove` needs no
  head and no first-node special case), `hlist_insert`, `hlist_remove` (resets the node),
  `hlist_linked`, `hlist_for_each`, `hlist_container`; `HASH_BUCKETS(bits)` buckets, zeroed =
  empty; `hash_64(value, bits)` = `(value * GOLDEN_RATIO_64) >> (64 - bits)` (unsigned, in range
  by construction). The table stores no keys: callers compare their own in the bucket walk.
- **Directories** (`<dirent.h>`, `libc/dirent/`, internals in `internal/dirstream.h`): `DIR` is
  opaque like `FILE`; `struct __dirstream` = `fd`, `pos`/`len` over a `_Alignas(uint64_t)` 2 KiB
  buffer of `getdents` records, and the one `struct dirent` (`d_ino`, `d_type`, `d_name[256]`)
  that `readdir` returns (overwritten by the next call, as POSIX allows). `opendir` = `open(path,
  O_RDONLY | O_DIRECTORY)` + `malloc` (`ENOMEM`, fd closed). `readdir` refills with `getdents`
  when `pos >= len` (`<= 0` → NULL: on the end `errno` is untouched, so callers set it to 0 to
  tell the end from an error), steps `pos` by `d_reclen` and copies the record out. `rewinddir`
  = `lseek` to 0 + empty buffer; `closedir` = `close` + `free`. `getdents(fd, buf, size)` is the
  raw syscall (not POSIX; Linux's `getdents64`). `DT_DIR`/`DT_REG`/`DT_CHR` are the uapi
  `MIHOS_DT_*` (BSD/Linux values); `ino_t` is `unsigned long` in `<sys/types.h>`.
  `<sys/sysmacros.h>`: `makedev`/`major`/`minor` over the uapi `MIHOS_MKDEV`/`MAJOR`/`MINOR`.

### malloc (`libc/malloc/`)

K&R-style free list on `sys/list.h`, single-threaded (no lock yet).

```text
┌─────────────────────┬────────┐┌──────────────────────┐
│ link (prev, next)   │ size   ││ payload ...          │
└─────────────────────┴────────┘└──────────────────────┘
▲ block_t (32 B, aligned(16))   ▲ block + 1: returned by malloc
link.next == MALLOC_MAGIC while allocated; size in UNITs (sizeof(block_t)), header included
```

- **Units:** a request of `n` bytes takes `(n + UNIT - 1) / UNIT + 1` units. Every block starts on a
  `UNIT` boundary (the heap starts page-aligned and grows in whole units), so payloads are 16-aligned.
  Pointer arithmetic on `block_t *` moves in units (`block + n`, `(block_t *)p - 1`).
- **Free list** (`__malloc_free_list`, a sentinel defined in `__malloc_insert.c`): free blocks sorted
  by address. `__malloc_insert` finds the first block above the new one, inserts before it (the
  sentinel when it is last), then merges with the next and the previous block when they touch
  (`a + a->size * UNIT == b`). A fully freed heap is one block again.
- **`malloc`:** `n == 0` → NULL; `n > SIZE_MAX - UNIT` → `ENOMEM`. First fit: an exact fit is
  unlinked; a bigger block gives its **tail** (`block + block->size` after shrinking), so the free
  part keeps its place on the list. No fit → `__morecore(units)`, then search again (cannot fail:
  the new memory merges with the last free block or is big enough alone). Allocations therefore come
  from the top of the heap down.
- **`__morecore`:** at least 64 KiB (`MORECORE_MIN_UNITS`) per `sbrk`; rejects
  `units > INTPTR_MAX / UNIT` (a cast to the signed increment would shrink the heap); the new memory
  becomes one free block through `__malloc_insert`. Memory is never returned to the kernel.
- **`free`:** NULL is a no-op; a header without `MALLOC_MAGIC` (bad pointer, double free: inserting
  overwrites `link.next`) prints to stderr and `abort()`s.
- **`calloc`:** `number > SIZE_MAX / size` → `ENOMEM` before multiplying; always `memset` (reused
  blocks are dirty). A zero count or size returns NULL like `malloc(0)`.
- **`realloc`:** NULL → `malloc`; size 0 → `free` + NULL; fits in the old capacity
  (`(size - 1) * UNIT`) → same pointer (a shrink keeps the whole block); else `malloc` + `memcpy` of
  the old capacity + `free`, the old block untouched on failure.
- Mixing `malloc` with direct `brk`/`sbrk` calls is unsupported (the break could lose its alignment).

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
  resource: `guard(irq)`, `guard(spinlock, &lock)` and `guard(mutex, &m)` (`sched/sync.h`; may
  sleep: never under a spinlock or in an IRQ handler). A guard state holds a *pointer* to the
  lock: a copy would be unlocked instead of the real one. Pitfalls: inside `scoped_guard`, `break`/
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
  so each `kprintf` is atomic and callable from IRQs, panic and early boot. Inside that section it
  first passes the raw bytes (UTF-8, before decoding) to `platform_debug_write`, so the host log
  gets every call whole and in screen order, panics and keyboard echo included. The PC `screen_glyph`
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
- `isr_common` called C with RSP 8 bytes off the SysV alignment (the CPU aligns to 16, then 21 qwords
  are pushed). Harmless until GCC compiled the rewritten `vsnprintf` with aligned SSE stores: the
  first `kprintf` inside a syscall (`process_exit`) took a #GP, and `panic` faulted again while
  printing, so the boot stopped silently. Fixed with `andq $-16, %rsp` before the call, and the
  kernel built with `-mgeneral-regs-only` (its SSE use also clobbered user vector registers, which a
  context switch does not save). Confirmed with `objdump -d ... | grep -c xmm`.
- `out_asm`/`in_asm` used `%b0`, forcing `%al`: `outw`/`outl` wrote one byte and `inw`/`inl` read
  one. Unnoticed while only `outb`/`inb` had callers; found when QEMU ignored the ACPI power off
  (`outw(0xB004, 0x2000)` wrote `0x00`). A plain `%0` takes the width from the operand.

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
- **4f-4 (done):** userland libc.
  - (a, done) uapi split: only `<mihos/...>` reaches the sysroot.
  - (b, done) `libk.a` / `libc.a` from one tree with separate flags.
  - (c, done) `crt0.o`, inline-asm syscall layer, `errno`, `unistd` wrappers, `exit`; `hello` on libc.
  - (d, done) stdio output over fds (`hello` prints with `printf`), `ferror` (test: `tests/stdio`).
  - (e, done) `address_space_unmap` + rollback in `address_space_map`, 64 KiB user stack, `SYS_BRK`
    with the heap after the ELF image, libc `brk`/`sbrk`, `malloc`/`free`/`calloc`/`realloc`
    (tests: `tests/brk`, `tests/malloc`, `tests/malloc_abort`).
- **Phase A restructure (done):** `user/` by category (`bin/`, `sbin/`, `tests/`) with discovery and
  install paths, the ISO mirroring the sysroot, `grub.cfg` modules named by install path with an
  expected status, `init_start` as a temporary test runner, the temporary `hello` tests as programs
  in `user/tests/`. The `user_program.S` kernel self-tests were moved later (the test-suite
  review after W6): all but the ring-3 loop, which only the kernel can stop.
- **stdio input (done):** `__fillbuf`, `fgetc`/`getc`/`getchar`, `ungetc`, `fgets`, `feof`,
  `clearerr` (tests: `tests/stdin`, `tests/stdin_interactive`).
- **Initial stack + spawn/wait (done):** `argc`/`argv`/`envp` on the System V stack, `crt0` and
  `environ`; `SYS_SPAWN`/`SYS_WAIT` with parent/children, orphans, fd inheritance and the POSIX
  wait status; `posix_spawn`, `waitpid`, `W*` (tests: `tests/args`, `tests/spawn`, the ELF
  self-test's stack checks). Decided: `spawn` now, `fork` + `exec` in phase C.
- **`/sbin/init` (done):** pid 1 reserved, `init_start` starts only init with its module string as
  `argv`, init runs one command, the test runner and its tables moved to `/usr/tests/run`,
  `BOOT_MODULE_MAX` 32. Decided: init's command comes from its `argv` (GRUB), init exiting powers
  off for now (debt), pid 1 reserved.
- **First shell (done):** `/bin/sh` and `/bin/echo`, libc string functions, `strdup`, `getenv`,
  `vsnprintf` rewritten with C99 semantics plus `snprintf`; the ISR alignment / kernel SSE fix.
- **printf test (done):** `user/tests/printf`; it found `hh`/`h` being parsed and ignored (fixed).
- **`wait` for any child (done):** `pid -1`, `child_exits` + `process_tree_lock`, libc `wait`.
- **Quiet tests (done):** a passing run prints only `self-tests: ok`, one `ok` line per test, the
  stdio ordering lines and the messages of the faults the tests cause on purpose.
- **Init modes (done):** `SYS_REBOOT` (init only), libc `reboot`, `/sbin/init once|respawn`,
  `init_start` panics if init exits.
- **Orphans to init (done):** re-parented in `process_exit`, reaped by init's `wait(-1)` loops.
- **Power off, debug console, getpid/getppid (done):** a test run ends with the emulator exiting;
  the whole output is on the host (`-debugcon`, `port_e9_hack`); the spawn test checks adoption.
  Found on the way: `outw`/`outl`/`inw`/`inl` only moved one byte.
- **Headless test script (done):** `./run-tests.sh`.
- **Shell extras (done):** quote-aware tokenizer, `$` expansion, history expansion.
- **C++ runtime (done):** constructor arrays, `__libc_start_main`, `atexit`/`__cxa_atexit`,
  `operator new`/`delete`, `__cxa_pure_virtual`. **Phase A is complete.**
- **Initramfs (done):** one ustar `initrd.tar` instead of a module per program, `initrd_find` in
  spawn, `init_start` and the ELF self-test, init's arguments on the kernel command line, programs
  without `.elf`, an initrd self-test.
- **VFS step (a) (done):** `vfs_node_t` and its ops, path lookup, the initrd as a tree mounted as
  `/`, `vfs_read_file` for spawn, init and the ELF self-test; a VFS self-test.
- **VFS step (b) (done):** `open`/`lseek` (`file_t` with `node` and `offset` under a position
  mutex), the mutex guard, libc `open`/`lseek`, the `files` test and the shared-offset case.
- **VFS step (c), first half (done):** inode numbers, the `readdir` node op, `SYS_GETDENTS`
  (entry-index offsets, records committed after the copy), `O_DIRECTORY`, libc `<dirent.h>`;
  the VFS self-test and the `dirs` test. Decided: file systems emit `.`/`..`; entries are a
  DTO (`vfs_dirent_t` → `mihos_dirent` → `struct dirent`), never nodes.
- **`ls` (done):** `/bin/ls` and libc `strerror`, `EXIT_SUCCESS`/`EXIT_FAILURE`.
- **Self-listing runner (done):** `/usr/tests/run` finds the tests with `readdir`. **VFS step
  (c) is complete.**
- **VFS step (d), `stat`/`fstat` (done):** node `mode`/`mtime` from the tar headers,
  `struct mihos_stat` (Linux x86-64 layout), `SYS_STAT`/`SYS_FSTAT`, the console as a character
  device, libc `<sys/stat.h>` translating field by field; the `user_path_copy` refactor; the
  VFS self-test section and the `stat` test. Decided: the full POSIX field set now (zeros where
  MiHOS has nothing yet), so mlibc's sysdep is a plain copy.
- **`isatty` and stdout buffering (done):** libc `isatty` (`ENOTTY`), stdout line-buffered only
  on a terminal (decided at its first write), `ls` listing file arguments; the `tty` test.
- **`/dev` step (a), mounts (done):** `mounted_root`/`mount_point` on nodes, `vfs_mount`, lookup
  crossing both ways, per-file-system `st_dev` (`vfs_dev_alloc`), an empty `dev/` in the
  initrd, devfs as an empty root mounted there; self-test and user test checks.
- **`/dev` step (b), devices (done):** `VFS_CHARDEV` nodes with `rdev` and the driver's
  `device_ops`, device opens with access modes, `st_rdev`/`DT_CHR`, `devfs_register`,
  `/dev/null` and `/dev/zero`, libc `<sys/sysmacros.h>`; the node put moved into `file_put`.
- **`/dev` step (c), the console (done):** `/dev/console` (5:1, `0600`), registered and opened
  by `console_init`, so init's fds 0–2 have a node; `vfs_file_stat` and its special case gone.
- **`/dev` step (d), `ioctl` (done):** `SYS_IOCTL` with size-encoded requests, the `ioctl` op,
  the console's `TIOCGWINSZ`, libc `<sys/ioctl.h>`, `isatty` as musl's (`/dev/null` is no tty
  any more); the `ioctl` test. **`/dev` is complete.**
- **Disk step 1, block layer + ATA PIO (done):** `block_register`/`find`/`read`, an ATA PIO
  driver (IDENTIFY over the four IDE slots, polled 28-bit reads), the ext2 test disk
  (`disk.img`, attached by `run-tests.sh`), the block self-test. Decided: ATA PIO first (Bochs
  and QEMU, no PCI or DMA); AHCI or virtio later only replace the driver. Not a prerequisite
  after all: "syscalls run with IRQs off" (a sleeping driver lets IRQs through; it costs
  latency, not correctness).
- **Disk step 2, block devices in `/dev` (done):** `VFS_BLOCKDEV` (`S_IFBLK` 0060000, `DT_BLK`
  6), `block_read_bytes`, `devfs_register_block` + `block_devfs_init`, `rdev` per block device
  (Linux's IDE numbers), `vfs_read` open to any node with a `read` op, libc `S_IFBLK`/`S_ISBLK`/
  `DT_BLK`; the block self-test's node checks and the `disks` test. Decided: a disk is read
  like a file (`vfs_file_ops`, seekable), not through `file_ops` like a character device (Linux's
  bdev inode); byte-to-sector logic lives in the block layer, devfs only forwards; disks are
  published after devfs exists, not at discovery; devices report `st_size` 0 (Linux), the size
  comes from `lseek(SEEK_END)`; read-only (`-EROFS`) until the driver writes.
- **Disk step 3, the block cache (done):** the hash table rewritten as Linux's hlist with a
  multiplicative `hash_64` (the old one was unused and broken), `list_move_after`, the
  `mutex_lock` declaration fix in `sync.h`, `block_cache_get`/`block_cache_put` over 64 leased 4 KiB
  buffers, `ENOBUFS`, `block_read_bytes` through the cache; the hash table and cache
  self-tests. Decided: no generic cache, but generic pieces (list, hlist) and each cache written
  for its keys and fill, as Linux does; cache blocks are 4 KiB whatever the file system's block
  size; xv6's two locks (a spinlock for the list and keys, a per-buffer mutex across the disk
  read); every buffer leased → `-ENOBUFS`, not sleeping; everything reads through the cache.
- **List and hlist names (done):** `list_remove`, `hlist_insert`/`hlist_remove`/`hlist_linked`.
- **Disk step 4, ext2 read-only (done):** `/mnt` in the initrd and a test disk with indirect
  blocks, holes, a long name and a symlink; the superblock and group checks, `ext2_node_get` and the
  node cache (`vfs_node_tryget`, `nlink` in the VFS), directory walks with checked records,
  `ext2_file_block` through three levels, reads with holes, `block_at` and the boot mount; the ext2
  self-test (with an overlay device for broken disks) and the `ext2` test. Decided: Linux's
  feature rule (incompat refuses, ro_compat/compat ignored read-only); `FILETYPE` and revision 1
  required; on-disk structs read in place; one node per inode, freed at the last put, never
  revived from 0 (`tryget`, so `release` runs once per node); a node holds its parent; only
  regular files and directories exposed; the first ext2 disk mounted on `/mnt` at boot.
- **Disk step 5, writing (in progress).** Decided: six sub-steps, each committed and tested
  alone; write-back (not write-through: W5 rewrites the same bitmap and superblock blocks on
  every allocation), dirty buffers written on eviction, on `sync()` and at `system_halt`, no
  flusher thread; whole cache blocks written (a small write fills the buffer first); locks
  node `lock` → `volume->lock` → the cache's; a writable mount only when every `ro_compat`
  feature is known (`SPARSE_SUPER`, `LARGE_FILE`), `s_state` not clean at mount, clean at the
  halt's sync; no clock (times unchanged); `e2fsck -fn` (in Docker) on a disk written without
  `-snapshot` as the referee for W5/W6; Bochs's disk `mode=volatile` (like QEMU's `-snapshot`).
  - **W1 (done):** `block_write`/`block_flush`, `ata_command`, ATA `WRITE SECTORS` and `CACHE
    FLUSH`; the block self-test's write section.
  - **W2 (done):** `fill_lock` → `data_lock`, `dirty`, `block_cache_begin_write`/`end_write`,
    write-back on eviction (pin under the old key, write without the spinlock, unpin, claim
    again), `block_cache_sync` at `system_halt`; the block self-test's write-back section.
  - **W3 (done):** `ENOSPC`, `block_write_bytes`, the `write` node op, `vfs_write`,
    `vfs_file_write`, the access mode for every node in `vfs_open` (`-EISDIR`, `-EROFS` only
    without a `write` op), devfs block nodes writable, `sys_write` counting what was written;
    the block self-test's node writes and the `disks` test's writes. Decided: a disk's end is
    Linux's (cut short across it, `ENOSPC` at it, 0 bytes always 0).
  - **W4 (done):** a node `lock` (reads and writes), `ext2_block_write`, `ext2_write` (stops
    at the end and at holes: a short count or `-ENOSPC`), a volume `read_only` with an unknown
    `ro_compat` feature or a disk that cannot write (`ext2_read_only_ops`); the ext2 self-test's
    writes and overlay cases, the `ext2` test's writes.
  - **W5 (in progress):** W5a (done): `first_data_block`/`blocks_per_group`, `n->blocks`,
    `ext2_inode_byte`, `ext2_disk_write`, `ext2_inode_store`/`ext2_inode_write`, the inode
    self-test. W5b (done): the groups-fit-their-bitmaps mount check, `ext2_block_alloc`/`free`
    under `allocation_lock`. W5c (done): `ext2_slot`, `ext2_file_block` creating blocks and
    tables, `ext2_write` filling holes and growing files, `max_file_size`/`EFBIG`; `ext2.c`
    reordered bottom-up in sections; `grow/` on the test disk; the growth tests;
    `./fsck-test.sh` (clean on the first run; folded into `run-tests.sh` after W6). W5d (done): halt hooks, `s_state` not clean while
    mounted writable and clean again at halt; the self-test reads it, `fsck-test.sh` asks
    `dumpe2fs` for `clean`. **W5 is complete.**
  - **W6 (done).** Decided: sub-steps W6a truncate, W6b the inode allocator, W6c
    directory entries + `O_CREAT`/`O_EXCL`, W6d `unlink` (freed at the last node put when
    the link count is 0) + `O_APPEND`, W6e `mkdir`/`rmdir`, W6f a full-disk test, then the
    split of `ext2.c`; no clock (new inodes get the parent's `mtime`, a deleted one the
    superblock's `s_wtime` as `i_dtime`: 1 read as an orphan-list link to `e2fsck`); permissions stored, never checked; a directory with `EXT2_INDEX_FL` is `-EROFS` for
    changes; names 1–255 bytes, `.`/`..` never created or removed; new 256-byte inodes get
    `s_want_extra_isize`; syscalls numbered from 21. W6a (done): `ext2_truncate`,
    `vfs_truncate`, `O_TRUNC`, `ftruncate`/`truncate`; the truncate tests. W6b (done):
    `s_first_ino`/`s_want_extra_isize`, `ext2_inode_alloc`/`init`/`free`, `ext2_disk_zero`.
    W6c (done): the `create` node op, `vfs_create`, `O_CREAT`/`O_EXCL` and `open`'s mode,
    `creat`; `ext2_dir_add`, `ext2_create`, directories walked under their lock; the create
    tests. W6d (done): the `unlink` node op, `vfs_lookup_parent`/`vfs_unlink`, `SYS_UNLINK`
    23 and libc `unlink`; `ext2_dir_remove`, `ext2_unlink`, `ext2_release` freeing an unlinked
    node at its last put; `O_APPEND` through `VFS_APPEND`; the unlink and append tests. W6e
    (done): the `mkdir`/`rmdir` node ops, `vfs_mkdir`/`vfs_rmdir` (trailing slashes, the dots
    refused by name), `SYS_MKDIR` 24 / `SYS_RMDIR` 25, libc `mkdir`/`rmdir`, `EMLINK`/
    `ENOTEMPTY`; `ext2_mkdir`, `ext2_rmdir`, `ext2_dir_empty`, `ext2_links_store`; the
    directory tests. W6f (done): the full-disk test; it found `ext2_inode_free` leaving the
    link count of a create's or mkdir's failed inode (fixed: freed inodes get 0 links).
    **W6 is complete: ext2 writing (W1–W6) is done.**
  - **Test-suite review (done, after W6).** Each test against what else covers it now: most
    overlaps are a kernel and a user test of one feature at different layers (the kernel one
    sees references, nodes, raw disk bytes; the user one the syscalls), kept. Removed: the raw
    user-mode programs the libc tests cover (their kernel-address `EFAULT` and `hlt` checks
    moved to `files` and `spawn`, plus closing twice and a write past the fd table), and
    `ext2_test_inode_write` with `ext2_inode_write` (every grow/truncate step reads the inode
    back; e2fsck checks them all). `fsck-test.sh` folded into `run-tests.sh`: e2fsck after
    every run. Left: `user/tests/stdio`'s output order is checked by eye (debt, until pipes);
    `selftest/ext2.c` (1,400 lines) could split like `fs/ext2/` if it gets in the way.
    Overall: inode allocation (the inode bitmap, `free_inodes`, `bg_used_dirs_count`),
    `O_CREAT`/`O_TRUNC`/`O_EXCL`, `unlink`, `mkdir`, `rmdir`, directory entries inserted in a
    record's slack and removed by growing the previous `rec_len`, truncate (frees blocks: the
    tests can clean up), `O_APPEND`, a full-disk `ENOSPC` test. The split of `ext2.c` came
    early, before W6e (done): `fs/ext2/`, one file per section plus `internal.h`, in nine
    buildable commits, each built and tested; every function moved unchanged (checked by a
    script: 44 bodies found once each).
- **After writing (pick an order):** partitions (MBR/GPT → `hda1` on the reserved minors,
  mounting a partition); IRQ-driven ATA (sleep instead of polling); the root on the disk.
  Phase C (the process model) can also start: none of these block it.

### Shell (`user/bin/sh/`)

```text
main.c   loop: prompt "mihos>" ─▶ command_read ─▶ history_expand ─▶ history_add ─▶ command_parse
         ─▶ command_run
read.c   command_read: fgets, '\n' stripped (READ_EOF on NULL); command_parse: the tokenizer
         (quotes, escapes, $ expansion) → READ_COMMAND / READ_NOTHING
run.c    command_run: builtin_run, else program_run
builtins.c  table {name, function, help}; exit [n], help, history
history.c   ring buffer of strdup'd lines
sh.h / history.h   shared types and declarations ("..." includes)
```

- **State:** `shell_t` (owned by `main`, passed down) holds `status` (last command's: `exit`'s
  default), `running` (cleared by `exit`) and the `history_t`. Modules take only the part they own
  (`history_add(history_t *, line)`); `main` wires them. The tokenizer never modifies the line.
- **Tokenizer** (`command_parse(sh, cmd)`): a state machine (`STATE_UNQUOTED`, `STATE_SINGLE`,
  `STATE_DOUBLE`) over `cmd->command` writes the words, each `'\0'`-terminated, into
  `cmd->words`; `argv` points there. `parser_t` holds `pos`, `start` and `in_word` (a word is open
  even when empty, so `''` and `""` make empty arguments) and `error`. Helpers: `word_open`,
  `word_append` (bounds check, keeps a byte for the `'\0'`), `word_append_string`, `word_close`
  (`MAX_ARGS`, stores the word). Unquoted: space/tab closes a word, quotes switch state without
  closing it (`"a b"x` is one word), `\` makes the next character literal. Single quotes: all
  literal. Double quotes: `\` escapes only `"` `\` `$`. An open quote at the end is
  `sh: unmatched quote` (no continuation lines).
- **`$` expansion** (`expand`, unquoted and in double quotes): `$?` (`status`), `$$` (`getpid`),
  `$NAME` (`NAME_START` then `strspn` over `NAME_CHARS`, `getenv`, nothing if unset, so a bare
  `$NOPE` makes no word); anything else is a literal `$` (`$5`, a trailing `$`). No field
  splitting: a value with spaces stays one word.
- **History expansion** (`history_expand(history, line, out, size)`, before `history_add`): copies
  the line, replacing `!!`, `!n` (bash number, as `history` prints it) and `!-n` anywhere outside
  `'...'` (`reference_length` decides what is a reference; a `!` before anything else is literal).
  Returns 1 (expanded: `main` prints the line and stores the expanded form), 0, or -1 with the
  error printed (`event not found`, `line too long`: the line is neither run nor stored).
- **Builtins:** function pointers `int (*)(shell_t *, int argc, char *argv[])`; `builtin_run`
  returns 1 if it ran one (its return value becomes `status`), 0 if not. `exit` with too many
  arguments returns 1 and keeps running; `exit n` uses `atoi` (no number check yet).
- **History:** `lines[HISTORY_MAX]` (32) `strdup`'d copies, oldest at `lines[start]`, `count`
  stored, `total` ever added; full → free the oldest and advance `start`. Blank lines are skipped.
  `history` prints entry `i` as number `total - count + 1 + i` (bash's numbering).
- **Programs:** a name with `'/'` is spawned as given; otherwise each `PATH` entry is tried as
  `dir/name` (`snprintf` with `%.*s`, so `PATH` is never cut up; empty entries skipped). `ENOENT`
  moves to the next entry. Not found → `sh: name: not found`, 127; other spawn errors → 126; killed
  → message and 128 + signal. Children inherit fds and `environ`.
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
- The initrd module stays reserved forever (files are used in place; never returned to the
  allocator).
- Modules far above the kernel waste the free RAM between them (one boundary, not per-module holes).
- Initrd directories are linear lists (`initrd_find_child` compares every name). No symlinks or hard
  links (skipped), no file modes used (everything is executable), read-only.
- `vfs_read_file` copies every program once more (archive → `kmalloc` buffer → address space);
  fine at ~100 KiB, replaced by mapping later. `kmalloc` above 1024 B rounds to a power-of-two
  number of pages.
- ATA: polling only (the CPU spins for every sector, with IRQs off inside a syscall), 28-bit
  LBA (128 GiB), no DMA; no check that the drive supports `CACHE FLUSH` (IDENTIFY word 83 bit
  12; Bochs and QEMU do; an old drive would abort it and the flush returns `-EIO`); a write
  error leaves the sectors of that command in an unknown state (no retry, no report of how far
  it got); IDENTIFY's model string unused; names by IDE
  position (`hda`…`hdd`), so the same disk is `hdb` on Bochs. The block table has no lock and
  no unregister. No PCI enumeration yet (AHCI and virtio need it).
- Block cache: a fixed 64 buffers (no growing or shrinking under memory pressure); `-ENOBUFS`
  when all are leased (a caller holding many blocks at once, e.g. a deep indirect walk, must put
  as it goes); the LRU walk is linear and skips leased buffers; no read-ahead; `refs` and the
  counters under one global spinlock (fine on one CPU). A failed read is retried by the next
  get but counted as a hit. Write-back: no flusher thread (dirty data reaches the disk only on
  eviction, a sync or the halt: a crash loses it all, and no `sync`/`fsync` syscall yet); a
  dirty buffer whose write-back keeps failing stays at the back of the LRU and fails every miss
  that reaches it (no skipping to the next free buffer, no error reporting beyond the get);
  a write-back writes all 8 sectors even if one byte changed; sync walks all 64 buffers and
  flushes every disk, written or not.
- The minors reserved for partitions (`hda1` = 3:1) are unused: no partition table is parsed.
  `block_read_bytes` and `block_write_bytes` return an error even after moving part of the
  range (POSIX wants the partial count; a written part stays written). A write covering a
  whole cache block still reads it from the disk first (no "get without fill").
- devfs: devices register at boot only (no lock, no unregister, nodes never freed); one flat
  directory; `devfs_readdir` walks the list from the start per index (O(n²) listing).
  Character devices refuse `lseek` (`ESPIPE`) where Linux lets `/dev/null` succeed.
- `ioctl`: one request (`TIOCGWINSZ`); integer arguments (requests whose third argument is no
  pointer, e.g. `FIONBIO`) would need the syscall layer to skip the copy (`MIHOS_IOC_NONE`
  passes the buffer, unused). Our request numbers differ from Linux's (`0x5413`), fine since
  only libc sees them. The console's size is the VGA text size, fixed.
- Mounts are two node pointers, set at boot: no `umount`, no lock (a mount after user space
  starts would race lookups), no mount over `/`. No current directory (absolute paths only).
- `read` moves at most 256 bytes per syscall (`SYSCALL_CHUNK_SIZE`): correct (a short read) but
  slow for files; no `fread`/`fopen` in libc yet. `sys_write` copies 256-byte chunks too (one
  backend call each) and stops at the first short one.
- `stdio.h` gets `O_*` from `<mihos/fcntl.h>` along with `SEEK_*` (POSIX keeps `O_*` out of
  `stdio.h`); split `SEEK_*` into their own uapi header if it matters.
- A program removed from `user/` stays in the sysroot, and so in the initrd, until `make clean`;
  the initrd is only rebuilt when an existing program changes.
- Fixed 64 KiB user stack (`exec_load`); no stack growth. The guard page below it is only "never
  mapped" (`brk` stops short of it), not a reserved region.
- `NX` still off, so `PF_X`/`MMU_EXEC` from the loader are informational.
- The `stdio` test's output order is only checked by eye (no pipes to capture it).
- Stale programs stay in the sysroot after their directory is removed (`make clean` to drop them).
- `tty_write` holds IRQs off for the whole write plus a full 4000-byte VGA copy; long user writes
  will need a mutex and a separate panic path.
- Screen redraws the whole buffer on every write; scrolling uses `memcpy` on an overlapping range.
- Single tty UTF-8 decoder state; relies on every write going through the IRQ-off section.
- Keyboard LEDs never follow Caps/Num Lock (`0xED` not sent; needs a PS/2 command path).
- No raw mode / termios; Tab, Esc, arrows and Home/End are ignored in line editing.
- Second-port interface test runs even when no second port was detected (may hang on
  single-channel controllers).
- `printf`/`vfprintf` format into one `BUFSIZ` stack buffer: longer output is cut (the return
  value now shows it, nothing handles it yet). `vsnprintf` has no `#`, `'`, `j`/`t`, floats or `%n`.
- Tabs on the console move the cursor 8 cells without writing them and ignore tab stops
  (`screen_put`), so tab-indented lines can look broken; the shell's `help` uses one.
- Faults the tests cause on purpose (the `null`/`hlt` self-tests, the spawn test's `segv` child)
  still print their fault messages; the user self-test's `files` program prints `Files test: stdout
  still works`. The stdio test's last line has no `'\n'` (it checks the flush at `exit`), so the
  runner's `test /usr/tests/stdio: ok` follows on the same line.
- The shell has no arrow-key history (needs raw mode / termios), no `;`, `&&`, pipes or
  redirections, no field splitting, no continuation lines for an open quote; a `'` inside `"..."`
  flips history expansion's quote tracking. Its features are checked by hand until pipes let a
  test feed it a script.
- stdio: no `fread`, `fopen`/`fclose`/`setvbuf`,
  `__stdio_head` is `const` (must change when `fopen` adds streams), no stream locking (needed with
  user threads); `stdio.h` is still partly a stub (`fopen`, `fread`, `fseek` declared but missing).
  `fgets` copies byte by byte through `fgetc` (could copy `[pos, len)` up to the `'\n'` at once).
  Switching one stream from reading to writing drops unread input (`__fwritex` resets the buffer);
  fine for the console, needs `fseek` semantics once files exist.
- Bochs (win32 GUI) sends Left Ctrl for the ABNT2 `/ ?` key, so `KEY_RO` can only be tested on QEMU.
- No IDT gate uses the IST stacks yet; a double fault on a broken kernel stack still triple-faults.
- `mmap_split_region` drops region remainders smaller than 16 frames.
- `address_space_unmap` (and so a `brk` shrink) frees pages but not emptied page tables (same as
  `page_unmap`); they go with the space.
- No self-test exercises the rollback in `address_space_map` (frame exhaustion midway): it would need
  all RAM; a free-frame count before/after a forced failure would do.
- malloc never returns memory to the kernel (a free block ending at the break could be trimmed with a
  negative `sbrk`); `realloc` never shrinks or grows in place (the freed tail or a free next block
  is not used); first fit walks the whole list (no size classes); no lock (needed with threads).
- The malloc test hard-codes the 32-byte header (`sizeof(block_t)`).
- Syscalls run with IRQs off for their whole duration (interrupt gate); long writes delay IRQs.
- The fd table has no lock (safe only with one thread per process and no `fork`).
- No self-test proves memory comes back: compare free frames (and the console file's refs) before
  and after `user_selftest` to catch a missing `process_release` / `file_put`.
- Fault messages print the thread id, not the pid.
- `process_wait(NULL)` returns -1, which is also a valid exit status (should assert).
- `wait` has no `WNOHANG` and no process groups (`pid` 0, `< -1` give `-ECHILD`); waking every
  waiter on any child's exit is fine with one thread per process.
- `spawn` copies user strings byte by byte; path limit is 64 bytes (`-E2BIG`, POSIX would say
  `ENAMETOOLONG`); programs come from the initrd only; no `posix_spawnp` (`PATH` search).
- Power off works only on Bochs and QEMU (fixed ports); real hardware halts (needs the ACPI FADT).
  A `panic` still halts with `cli; hlt` (Bochs shows a popup), on purpose: the screen stays.
- Bochs reports the shutdown port as `>>PANIC<<` and exits with code 1 (its convention).
- The crash loop guard counts kills in a row, not kills per time (no clock in user space).
- The adoption test's grandchild cannot be waited for: a failure is only its printed `FAIL` line,
  not counted by the runner.
- `stat` has no clock behind it: all three times are the file's `mtime` (ext2's `i_atime`
  and `i_ctime` are not read); no users (`uid`/`gid` 0, ext2's `i_uid`/`i_gid` ignored);
  `nlink` 1 outside ext2. No `lstat` (no symlinks) or `fstatat` (no current directory).
- `ls` prints file arguments in argument order, mixed with the directories (real `ls` prints
  files first, then each directory). Its options, several paths and errors are checked by
  hand, like the shell.
- `initrd_readdir` walks the children list from the start for every index, so a whole listing
  is O(n²); a file system could use `offset` as an opaque cursor (ext2: a byte position).
- `getdents` fills at most a page per call; `d_type` is `DT_DIR`/`DT_REG`/`DT_CHR`/`DT_BLK`
  (no symlinks, FIFOs or sockets yet). The `dirs` test hard-codes the root's layout (`bin`, `dev`, `mnt`, `sbin`, `usr`).
- The self-tests run on `kmain`'s 16 KiB boot stack (`boot.S`), which has no guard page: an
  overflow corrupts the memory below it silently (it happened: `ext2_test_unlink` once had a
  15 KB frame). Kernel thread stacks are 16 KiB too. A guard page (unmapped page below each
  stack) would turn it into a page fault.
- The block self-test once failed reading 300 sectors at 7000 with `-EIO`, right after a
  build (host busy); not reproduced in ~20 runs since. Polling ATA's timeouts are loop
  counts, not times; IRQ-driven ATA is the real fix.
- `user/tests/stdio` checks its return values, but the output order (stdout line-buffered,
  stderr unbuffered, the flush at exit) only by eye: it passes whatever the order. Checkable
  once pipes exist (phase C): a child's stdout and stderr read back in order.
- ext2 (each item becomes work when something needs it):
  - no `rename`; no `link`, so a file has one name (but the
    link count is kept right for when it has more); symlinks (and other kinds the VFS cannot
    show) cannot be unlinked (a fast symlink's `i_block` holds text); an `O_APPEND` file's
    offset is the size read after its write, so another appender's bytes may come before it
    (Linux returns the exact end from inside the write); a failure to free an unlinked file at
    its last put only logs (a leak); times are never
    updated (no clock), so a created file gets its directory's mtime and the directory's own
    times stay; no umask; permissions stored, never checked; `s_mnt_count`,
    `s_mtime` and `s_wtime` are not written; a not-clean or erroneous volume only warns; the
    halt hook marks the volume clean before the cache sync, and the sync may sleep (a mutex),
    so another process could still write between the two (nothing stops other processes at
    halt yet); a read-only volume is decided at mount only;
  - allocation: a linear first-fit bitmap scan per call (no preallocation or reservation
    windows, so two files growing at once interleave their blocks); `s_r_blocks_count`
    (reserved for root) ignored; only the primary superblock and descriptors are updated; a
    write error after the bitmap bit is set (descriptor or counts) loses that block until
    `e2fsck`; no ordering of metadata writes (consistent only after a sync, ext2's model); a
    new data block is zeroed even when the write fills all of it; `s_r_blocks_count` aside,
    a full disk is filled to its last block (the full-disk test does); truncate frees block by block (one bitmap write and two counter writes each, no
    batching) and walks every entry of a table it enters; inodes are first-fit from the
    parent's group (no Orlov spreading of directories across groups, which matters only on
    multi-group disks);
  - directories: a create walks the directory twice (the name check, then the room search),
    linearly; a new block is only ever added at the end (a hole in a directory is skipped,
    never filled); if writing the new block's record fails, the block stays linked with the
    size unchanged (`e2fsck` mends it); a create whose `ext2_node_get` fails returns the
    error though the file exists; the directory's lock serializes every lookup, listing,
    create and unlink in it; an unlink walks the directory twice too (the name, then the
    removal); a directory never shrinks (emptied blocks stay, holding one unused record);
    `mkdir`'s last step (the parent's `i_links_count`) failing leaves the directory made with
    the parent's count one low (`e2fsck` mends it); rmdir needs the directory's name
    (removing one through a path ending in `/.` is refused by the VFS, as Linux does);
  - one node `lock` for reads and writes: two reads of a file wait for each other (an rwlock
    would let them share);
  - symlinks, device files, FIFOs and sockets on the disk are not shown or found (no VFS node
    types for them; symlinks with `readlink` and path resolution in phase C);
  - revision 0 and disks without `FILETYPE` are refused (readdir would have to read each
    inode for its type); blocks over 4 KiB are refused (they would straddle cache buffers);
  - `readdir` counts from the first entry on every call (O(n²) per listing; a byte-offset
    cursor fixes it); `lookup` is a linear scan (the `dir_index` hash trees are ignored);
  - no unmount: a mounted file system stays allocated forever, also after a failed
    `vfs_mount`, and the self-test's overlay mounts leak the same way; one disk is mounted,
    at boot only (no `mount` syscall), always on `/mnt`;
  - `i_flags` (immutable, append-only, compressed, `EXT2_INDEX_FL`) and extended attributes
    are ignored; `s_state` (not cleanly unmounted, errors) is not checked;
  - `volume->lock` covers the whole `ext2_node_get`, including its disk read: lookups of different new
    files wait for each other.
- `sys_exec` (replacing the running image) is not implemented: comes with `fork` in phase C.
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
x86_64-mihos-readelf -l sysroot/usr/tests/hello
```

`make all` also builds `disk.img` (needs `mke2fs`: rebuild the Docker image once after a
Dockerfile change with `docker build . -t mihos-compiler` from `cross-compiler/`, **not**
`docker-build.bat`: its `robocopy` refreshes the image's sysroot copy, which invalidates the
cache before the GCC build and rebuilds the whole cross-compiler).

Run Bochs (`port_e9_hack: enabled=1` in `.bochsrc` sends the output to its console; the window
closes by itself at `system halted`; the disk needs `ata0-slave: type=disk, path=disk.img,
mode=volatile`, or the block self-test panics; `volatile` drops the writes at exit, like QEMU's
`-snapshot`, where `flat` would keep them in `disk.img`):

```sh
bochs -q -f .bochsrc
```

Run QEMU (verifies keys Bochs drops; `cpu_breakpoint` is a no-op there). `-debugcon stdio` copies
the output to the terminal; `-no-shutdown` keeps the window after the power off:

```sh
qemu-system-x86_64 -cdrom mihos.iso -drive file=disk.img,format=raw,if=ide,index=0 -snapshot -m 1024 -no-reboot -no-shutdown -debugcon stdio
qemu-system-x86_64 -cdrom mihos.iso -drive file=disk.img,format=raw,if=ide,index=0 -snapshot -m 1024 -no-reboot -no-shutdown -d int,cpu_reset -D qemu.log
```

Automatic test run, no window (about 10 s; exits 0 only after a clean power off with
`tests: N passed, 0 failed` and no `FAIL` line; prints the log otherwise, kept in `run-tests.log`):

```sh
./run-tests.sh
```

The run writes to `build/test-disk.img`, a fresh copy of `disk.img` (the image itself never
changes), and after the tests checks it with `e2fsck -fn` and `dumpe2fs`'s state in the
`mihos-compiler` image: it passes only if the disk is consistent and the halt left it clean.
`FSCK=auto` (default) skips that, saying so, without Docker or the image; `FSCK=1` requires
it, `FSCK=0` turns it off. The copy stays for `debugfs` after a failure.

The same by hand (on Windows the log path must be a Windows path):

```sh
qemu-system-x86_64 -cdrom mihos.iso -drive file=disk.img,format=raw,if=ide,index=0 -snapshot -m 1024 -no-reboot -display none -debugcon file:run.log
```

Local, intentionally uncommitted: `.bochsrc`, `bx_enh_dbg.ini`.
