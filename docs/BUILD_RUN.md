# Build & Run

All build paths assume Docker mount `repo -> /root/env`. Native Windows `make` fails (`rm/cp --preserve`, `/root/...`, `grub-mkrescue`).

## Toolchain

- Default `HOST=x86_64-mihos`, `CC=x86_64-mihos-gcc` — `common.mk:8-15`
- Flags `-O2 -g -std=gnu11 -mcmodel=kernel -mno-red-zone -mno-ms-bitfields -Wall -Wextra` — `common.mk:17`
- `ASFLAGS=-f elf64` dead (GCC assembles `.S`, not NASM) — `common.mk:23`
- `ARCH=x86_64` hardcoded — `common.mk:1`
- Custom fork: `cross-compiler/Dockerfile:1-43`, `build-binutils.sh:1-16`, `build-gcc.sh:1-18`
- Generic fallback `randomdude/gcc-cross-x86_64-elf` mismatches default `HOST` — `generic-cross-compiler/Dockerfile:1-12`
- Build/run wrappers: `cross-compiler/docker-build.bat`, `cross-compiler/docker-run.bat:1` (`-v "%cd%/..:/root/env"`)

## Source -> ISO

Root fan-out `Makefile:1-4`: `libc -> kernel -> grub` (note `all: $(libk)` expands empty at root, ordering comes from recipes).

1. libc `libc/Makefile:1-24`: `libk.a + libc.a` via `ar rcs $@ $?` (changed-only, not `$^`), install to `/root/env/sysroot/usr/{include,lib}`
2. kernel `kernel/Makefile:24-42`: link `crtbegin + objects + libk + -nostdlib -lk -lgcc + crtend` with `-n -T linker.lds.i`; `linker.lds` preprocessed `CC -E -P`; `crtbegin/end` copied via `print-file-name`; install headers + `boot/kernel`
3. grub `grub/Makefile:14-18`: `cp grub.cfg`, `cp kernel -> isodir/boot/mihos.kernel`, `grub-mkrescue /usr/lib/grub/i386-pc -o ../mihos.iso`
4. Boot contract `grub/grub.cfg:1-7`: `multiboot2 /boot/mihos.kernel`

Recursive sources via `include_dir` macro — `common.mk:40-57`; objects under `build/x86_64/`; `-MD` deps — `common.mk:70-77`. Manifests: `kernel/subdir.mk:1-6`, `libc/subdir.mk:1-5`, per-dir `*/subdir.mk`.

`dump: objdump -d kernel/build/x86_64/kernel > mihos.txt` — `Makefile:23-24`.

## Run / debug

- `run.bat:1`: `qemu-system-x86_64 -cdrom mihos.iso`
- `run-debug.bat:1`: `-no-reboot -no-shutdown -d int -monitor stdio`
- Bochs `.bochsrc:1-11`: cdrom boot, `magic_break enabled`, `log bochslog.txt`; `megs:32` conflicts `memory: guest=1024`; `bx_enh_dbg.ini` GUI prefs only
- Kernel magic breakpoint `kmain.c:28-29`: `xchgw %bx,%bx`
- No `launch.json`, GDB stub, `symbol-file` wiring. VSCode IntelliSense points at `sysroot/usr/include` — `.vscode/c_cpp_properties.json:7`

## Ephemeral vs source

Ephemeral (ignored but present locally): `build/`, `sysroot/`, `isodir/`, `mihos.iso`, `mihos.txt`, `bochslog.txt` — `.gitignore:45-64`. Seed snapshot: `cross-compiler/sysroot/` — `cross-compiler/.gitignore:1-2`.
