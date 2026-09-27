# Console

Live path:

```text
kprintf(fmt,...) -> kvprintf -> vsnprintf(str,512) -> tty_write
  -> screen_put (backbuffer) -> screen_update (VGA flush)
```

- `kernel/kprintf.c:5-21`, buffer `KPRINTF_BUFFER_SIZE 512`
- `kernel/driver/tty.c:5-19`: `tty_putchar -> screen_put`, then `screen_update`
- `kernel/driver/screen.c:3-56`: printable `0x20-0x7E`, `\n \t`, wrap at 80, scroll via `memcpy/memset`
- `kernel/arch/x86_64/screen.c:5-13`: flush 2000 chars to `0xB8000` as `WHITE on BLACK`; `pos` computed unused; no cursor port I/O
- Contract: `include/driver/screen.h:11-52`, `SCREEN_WIDTH 80 HEIGHT 25 TAB 8`

## Panic

`kernel/sys/panic.c:4-10` + `include/panic.h:1-10`: `kvprintf` then single `cli;hlt`. Wakes fall off end. Should loop. Marked TODO.

`vsnprintf` calls `panic` on precision / unknown specifier — `libc/stdio/vsnprintf.c:90,154`. Format typo halts kernel; recursion risk `panic->kvprintf->vsnprintf->panic`.

## Gotchas

- `kmain.c:14` prints before MM; `tty_init` (`driver/tty.c:21-23`) is never called. Works by `.bss` zero accident.
- `tty_write` returns count but callers ignore; `tty_putchar` always 0.
- `vga.c` empty — `arch/x86_64/vga.c:1`.
- Formatter supports `%d%i%u%p%x%X%s%c%%`, `0` flag + width, no precision — `libc/stdio/vsnprintf.c:20-166`.
