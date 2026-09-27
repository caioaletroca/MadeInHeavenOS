# libc vs libk

Intended: `libk` freestanding kernel-safe, `libc` hosted userspace. Actual: both archives from same `objects` — `libc/Makefile:3-14`, `common.mk:42-57`.

- `ARCH_FREEOBJS / ARCH_HOSTEDOBJS` empty, unused — `libc/arch/x86_64/make.config:1-8`
- Same `CFLAGS -mcmodel=kernel -mno-red-zone` for all — `common.mk:17`
- `-D__is_kernel` set — `kernel/Makefile:6` — nothing checks it
- Kernel links `libk` — `kernel/Makefile:28-36`, `LDFLAGS_EXTRA -nostdlib -lk -lgcc` — `common.mk:24-25`
- Headers installed to sysroot — `libc/Makefile:16-18`, `kernel/Makefile:16-22`

## What's built

`libc/subdir.mk:1-5`: `stdtest stdio stdlib string`. Excluded by omission:

- `malloc/malloc.c`, `morecore.c` (empty stub) not built
- `stdio/putchar.c` exists, not in `stdio/subdir.mk:1-7`
- `stdlib/abort.c` exists, not in `stdlib/subdir.mk:1-3`

## Two output paths

Live kernel: `kprintf -> vsnprintf -> tty`, see `CONSOLE.md`.

Dead/hosted: `vprintf -> vfprintf -> fwrite -> fputc -> FILE` — `libc/stdio/vprintf.c:3-5`, `vfprintf.c:3-29`, `fwrite.c:4-21`, `fputc.c:4-6`, `FILE.h:4-8`, `FILE.c:4-12`. Never called by kernel. `stderr=&streams[3]` OOB, buffers NULL.

## String/stdlib notes

- `memcpy/memset` carry `optimize(no-tree-loop-distribute-patterns)` — `string/memcpy.c:4-10`, `string/memset.c:16-24`
- `memmove` forward-only, `TODO: Fix` — `string/memmove.c:3-20`
- `strcpy` misses NUL — `string/strcpy.c:3-14`
- `atoi` minimal `ctype` — `stdlib/atoi.c:4-30`, `include/ctype.h:1-10`
- `malloc.c:3-5` empty return (UB); kernel uses own `kernel/kmalloc.c:3-14`
- `errno.h` empty, `unistd.h` decl-only, `stdio.h:26-33` declares unimplemented `fopen/fclose/fflush/...`
- Header-only `sys/list.h:25-77`, `sys/hashtable.h:33-120`, `sys/io.h:8-16`
- Shared `BUFFER_SIZE 512` — `include/stdio.h:12`, `kernel/kprintf.c:3`

To split properly: populate `ARCH_FREEOBJS/HOSTEDOBJS`, gate with `-D__is_libk`, stop building both `.a` from identical objects.
