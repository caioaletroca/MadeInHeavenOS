# Known Bugs & Risks

Read code before fixing. Ordered roughly by blast radius.

## Memory

- `mm/mmap.c:20` skips last region (`i < length-1`), underflows if 0. Same pattern `mmap.c:42,64`.
- `mm/slab.c:238-242` `gfp_order=_fnzb(bytes)` but header says pages — `include/mm/slab.h:38-40`.
- `mm/slab.c:110` uninit `i`; `bufctl->buf/slab` never set; small-object reserves pointer not struct — `slab.c:114,207-213`; off-slab TODO — `slab.c:97-100,119-124`.
- `mm/slab.c:143` `list_container(&partial.next,...)` should take node, cf `buddy.c:56-59`.
- `mm/frame.c:11-20` links zone even if `zone_init` fails.
- Zone flags category-vs-mask mismatch — `include/mm/zone.h:9-37`, `mm/frame.c:26-29`.
- `kmalloc.c:3-13` physical-vs-virtual ambiguity; callers check NULL but it never fails. See `ADDRESS_SPACE.md`.
- Allocator can hand phys >1GiB with no direct map — `arch/x86_64/boot.S:235-244`, `mm/slab.c:79-87`.
- `mmap` arrays fixed 128, no bounds — `mm/mmap.c:5-15,117-128`; alignment math can overshoot — `mmap.c:175-179`.
- `paging.c:55-79` dereferences non-present entries; `page_map` diagnostic only, ignores phys/flags — `paging.c:33-97`; fault handler always panics — `paging.c:99-110`.

## Interrupts / drivers

- `include/driver/ps2.h:30-35` `static` state in header = per-TU copies.
- `arch/x86_64/ps2.c:87-91` second-port test sets `devices[0]`.
- `driver/keyboard.c:5-7,41` raw `inb/outb` + hardcoded EOI; `pic_send_EOI` unused — `arch/x86_64/pic.c:43-48`; busy-spin no timeout — `keyboard.c:36`.
- `isr_info.type` never read — `arch/x86_64/isr.c:9-16`; NULL = silent drop.
- `0x8F` for IRQs 32-255 — `arch/x86_64/idt.c:77-342` — verify DPL intent; syscall 128 TODO.
- `tty_init` never called; console works by BSS accident — `driver/tty.c:21-23`, `kmain.c:14`.
- `arch/x86_64/screen.c:6` `pos` unused; no cursor.
- `panic.c:9` single `hlt`, falls off on wake; `exceptions.c:40` panic-all.

## libc

- `vsnprintf -> panic` on precision/unknown — `stdio/vsnprintf.c:90,154`; `%s` unbounded, `%c` remain leak, `size==0` underflow — `vsnprintf.c:20-166`.
- `FILE.c:12` `stderr=&streams[3]` OOB; `fputc` no return/check — `stdio/fputc.c:4-6`.
- `vfprintf` uses `n` not `left`, `w<0` on size_t always false — `stdio/vfprintf.c:17-26`; `fwrite` `s` not reset — `stdio/fwrite.c:5-15`.
- `strcpy` misses NUL — `string/strcpy.c:3-14`; `memmove` forward-only — `string/memmove.c:3-20`.
- `malloc.c:3-5` empty return; `morecore.c` empty; `putchar.c`/`abort.c` unbuilt.
- `libk.a==libc.a` content — `libc/Makefile:9-10`.

## Build

- Root `all: $(libk)` empty var — `Makefile:1`.
- `ar rcs $@ $?` changed-only — `libc/Makefile:9-10`.
- `ASFLAGS` dead, `header.S:4` checksum TODO, `boot.S:53-56` EFLAGS question, Bochs `megs` vs `memory`, generic compiler HOST mismatch. See `BUILD_RUN.md`.
