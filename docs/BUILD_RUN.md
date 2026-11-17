# Build & Run

All build paths assume the Docker mount `repo -> /root/env` (`compilers/cross/docker-run.bat`).
Native Windows `make` fails (`cp --preserve`, `grub-mkrescue`, `mke2fs`). Starting from a
fresh clone, with no toolchain image yet: `BOOTSTRAP.md`.

User programs link against mlibc (`mlibc/`, a submodule) since roadmap D10d; the kernel
links `libk.a` (`libk/`), which never sees the sysroot.

## Output layout

Everything built goes under `build/<target>/`, one folder per target. The target is
`<ARCH>-<PLATFORM>` (`x86_64-pc` today); the toolchain triple (`HOST=x86_64-mihos`) is a
build setting, not a folder.

```text
build/
└── x86_64-pc/
    ├── images/      what runs: mihos.iso, disk.img (ext2 test disk), part.img (partitioned disk)
    ├── sysroot/     headers, libraries, installed programs (bin/, sbin/, usr/)
    ├── objects/     one folder per component: kernel/, libk/, mlibc/, user/, grub/ (+ initrd.tar)
    ├── staging/     trees that become images: iso/, initrd/, disk/, root/
    ├── test/        run-tests.sh: run-tests-live.log, run-tests-disk.log, test-disk.img and test-part.img (the last run's disk copies)
    └── mihos.txt    make dump (kernel disassembly)
```

- `common.mk` defines the folders (`OUTPUT_DIR`, `BINARY_DIR` = `objects/<component>`, the
  component being the directory make runs in, `SYSROOT_DIR`, `IMAGES_DIR`, `STAGING_DIR`);
  the root `Makefile` repeats the few it needs.
- `make clean` deletes `build/<target>/`, `make purge` deletes `build/`. A component's own
  `make clean` deletes its objects for the target, `make purge` for every target.
- The source tree has no build output; `.gitignore` only needs `/build`.

## Toolchain

- `HOST=x86_64-mihos`, `CC=x86_64-mihos-gcc` (`common.mk`); arch flags from `config/<arch>.mk`.
- Custom GCC/binutils: `compilers/cross/Dockerfile`, `build-binutils.sh`, `build-gcc.sh`,
  configured `--with-sysroot=/root/env/sysroot`. That path is only the compiler's default:
  every compile and link passes `--sysroot=$(SYSROOT_DIR)`.
- `compilers/cross/docker-build.bat` copies `build/x86_64-pc/sysroot` into the image before
  `docker build` (the GCC build needs the headers: `make toolchain-headers` makes them, the
  uapi and mlibc's, with no target compiler). GCC's startup files are mlibc's `crt1.o`, then
  `crti.o crtbegin.o` (`gcc/config/mihos.h` in the GCC fork). After a Dockerfile change, run
  `docker build . -t mihos-compiler` alone: the copy invalidates the cache and rebuilds GCC.

## Source -> images

`make all`: `headers -> libk -> kernel -> mlibc -> user -> grub`, then `disk.img` and `part.img`.

1. headers: `kernel/include/uapi` → `sysroot/usr/include`.
2. libk: `libk.a` (kernel flags) → `objects/libk/libk.a`, not installed. Its headers
   (`libk/include`) are the kernel's, with `-nostdinc` (`FREESTANDING_CPPFLAGS` in
   `common.mk`). Our libc's user side stays in the tree, unbuilt.
3. kernel: `crtbegin + objects + libk + -lgcc + crtend`, `-T linker.lds.i` →
   `sysroot/usr/boot/kernel`.
4. mlibc: meson + ninja in `objects/mlibc` → headers in `sysroot/usr/include`, `libc.a`,
   `crt1.o` and the empty `libm.a`, … in `sysroot/usr/lib`. `make mlibc-clean` starts over.
5. user: one program per directory under `bin/`, `sbin/`, `tests/` → `sysroot/{bin,sbin,usr/tests}`,
   static against mlibc, installed without debug info. `tests/cxx` waits for libstdc++ (D10e).
6. grub: `initrd.tar` (ustar of the programs plus the empty `dev/`, `mnt/`) and the ISO
   (`staging/iso` with `grub.cfg`, `mihos.kernel`, `initrd.tar`) → `images/mihos.iso`.
7. `disk.img`: `disk/` plus generated files, `mke2fs -d`.
8. `part.img`: `disk-tools/partition-disk.sh` (MBR, markers), then the initrd's tree as ext2
   on partition 1 (`mke2fs -E offset=`).

## Run / debug

- `./run-tests.sh`: headless QEMU, the automatic tests, e2fsck of the disk copy (`TIMEOUT`
  per boot, 600 s by default: the boot from disk takes over two minutes).
- `run.bat`: QEMU with the window, both disks, `-snapshot`.
- `run-debug.bat`: QEMU with `-d int -monitor stdio`.
- Bochs: `.bochsrc` (local), paths into `build/x86_64-pc/images/`.
- VSCode IntelliSense: `build/x86_64-pc/sysroot/usr/include` (`.vscode/c_cpp_properties.json`).
