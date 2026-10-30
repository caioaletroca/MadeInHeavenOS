# Build & Run

All build paths assume the Docker mount `repo -> /root/env` (`cross-compiler/docker-run.bat`).
Native Windows `make` fails (`cp --preserve`, `grub-mkrescue`, `mke2fs`).

## Output layout

Everything built goes under `build/<target>/`, one folder per target. The target is
`<ARCH>-<PLATFORM>` (`x86_64-pc` today); the toolchain triple (`HOST=x86_64-mihos`) is a
build setting, not a folder.

```text
build/
└── x86_64-pc/
    ├── images/      what runs: mihos.iso, disk.img (ext2 test disk), part.img (partitioned disk)
    ├── sysroot/     headers, libraries, installed programs (bin/, sbin/, usr/)
    ├── objects/     one folder per component: kernel/, libc/, user/, grub/ (+ initrd.tar)
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
- Custom GCC/binutils: `cross-compiler/Dockerfile`, `build-binutils.sh`, `build-gcc.sh`,
  configured `--with-sysroot=/root/env/sysroot`. That path is only the compiler's default:
  every compile and link passes `--sysroot=$(SYSROOT_DIR)`.
- `cross-compiler/docker-build.bat` copies `build/x86_64-pc/sysroot` into the image before
  `docker build` (the GCC build needs the headers). After a Dockerfile change, run
  `docker build . -t mihos-compiler` alone: the copy invalidates the cache and rebuilds GCC.

## Source -> images

`make all`: `headers -> libc -> kernel -> user -> grub`, then `disk.img` and `part.img`.

1. headers: `kernel/include/uapi` → `sysroot/usr/include`.
2. libc: `libk.a` (kernel flags) and `libc.a` + `crt0.o` (user flags) from one tree →
   `sysroot/usr/lib`.
3. kernel: `crtbegin + objects + libk + -lgcc + crtend`, `-T linker.lds.i` →
   `sysroot/usr/boot/kernel`.
4. user: one program per directory under `bin/`, `sbin/`, `tests/` → `sysroot/{bin,sbin,usr/tests}`.
5. grub: `initrd.tar` (ustar of the programs plus the empty `dev/`, `mnt/`) and the ISO
   (`staging/iso` with `grub.cfg`, `mihos.kernel`, `initrd.tar`) → `images/mihos.iso`.
6. `disk.img`: `disk/` plus generated files, `mke2fs -d`.
7. `part.img`: `disk-tools/partition-disk.sh` (MBR, markers), then the initrd's tree as ext2
   on partition 1 (`mke2fs -E offset=`).

## Run / debug

- `./run-tests.sh`: headless QEMU, the automatic tests, e2fsck of the disk copy.
- `run.bat`: QEMU with the window, both disks, `-snapshot`.
- `run-debug.bat`: QEMU with `-d int -monitor stdio`.
- Bochs: `.bochsrc` (local), paths into `build/x86_64-pc/images/`.
- VSCode IntelliSense: `build/x86_64-pc/sysroot/usr/include` (`.vscode/c_cpp_properties.json`).
