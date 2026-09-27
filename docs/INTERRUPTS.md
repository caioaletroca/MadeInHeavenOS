# Interrupts, PIC, PS/2, Keyboard

Bring-up: `kernel/cpu/interrupts.c:3-15`

```text
idt_init() -> pic_init() -> exception_init() -> idt_load()
```

Enabled later via `kmain.c:20`.

## IDT / ISR

- Table: `arch/x86_64/idt.c:18-25`, flags `0x8E` for 0-31, `0x8F` for 32-255 — `arch/x86_64/idt.c:77-342`
- Stubs: `arch/x86_64/isr_stubs.S:1-337`, `ISR` pushes dummy 0, `ISR_ERR` uses CPU code, packs `{err low, vector high}` into `info`
- Context must match `include/isr.h:7-30`
- Dispatch: `arch/x86_64/isr.c:9-16`, NULL handler = silent `iretq`
- Registration: `isr_set_info` — `arch/x86_64/isr.c:3-8`, type `ISR_EXCEPTION / ISR_IRQ` — `include/isr.h:40-46`, but `type` is never read

## Exceptions

`kernel/cpu/exceptions.c:41-68`: vectors 0-31 all `panic` with `rip/rsp/int_no/err`. No recovery. Vector 14 page-fault is overridden by `paging_init` — `kernel/mm/paging.c:112-119`.

## PIC

`kernel/arch/x86_64/pic.c:15-41`: remap `0x20/0x28`, cascade `4/2`, `ICW4_8086`. `pic_init` then masks to `0xFD/0xFF` — `pic.c:4-13`, leaving IRQ1 keyboard.

`pic_send_EOI` exists — `pic.c:43-48` — but keyboard uses hardcoded `outb(0x20,0x20)` — `kernel/driver/keyboard.c:41`.

## PS/2

Policy `kernel/driver/ps2.c:3-44`, mechanism `kernel/arch/x86_64/ps2.c:1-92`, contract `kernel/include/driver/ps2.h:1-77`.

Sequence: disable `0xAD/0xA7` -> flush -> unset IRQ+translation -> detect 2nd port -> self-test `0xAA` expect `0x55` else `panic` -> interface test `0xAB/0xA9` -> `ps2_keyboard_init` -> set IRQ bits -> enable `0xAE/0xA8`.

State bug: `static ps2_devices[2]` in header — `include/driver/ps2.h:30-35` — every TU gets private copy.

Second-port test writes `devices[0]` instead of `[1]` — `arch/x86_64/ps2.c:87-91`.

## Keyboard

`kernel/driver/keyboard.c:35-51`: registers vector 33 (`0x20+1`), spins on `0x64&1`, reads `0x60`, `kprintf SCANCODE`, manual EOI. No decode, queue, timeout, slave EOI. Duplicates `inb/outb` instead of `arch/x86_64/io.h:7-77`.
