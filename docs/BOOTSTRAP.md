# Bootstrapping MiHOS from nothing

How to get from a fresh clone and an empty Docker to a booting `mihos.iso`, and why each
step exists. Written for whoever starts from scratch: you, a contributor, or another OS
developer who wants to see how the toolchain loop gets closed.

Everything runs inside Docker containers, so the host only needs Docker and git. Building an OS
on the host directly means fighting its compiler, its `make`, its `cp` and its file system
tools; the containers pin all of that.

## The chicken and the egg

A compiler that builds programs *for MiHOS* has to know MiHOS: where its headers and libraries
are, which startup files to link, what `__mihos__` means. That is GCC's `x86_64-mihos` target.
But building that GCC needs the target's C headers (libgcc includes them), and those headers
are part of the OS the compiler is meant to build.

```text
   MiHOS's headers ──needed by──▶ x86_64-mihos GCC ──builds──▶ MiHOS (headers, libc, kernel, programs)
         ▲                                                                      │
         └──────────────────────────────── part of ◀────────────────────────────┘
```

Every OS breaks this loop the same way: start from something that does not know your OS yet,
use it to produce just enough of your OS, then build the real compiler on top. A self-hosting
compiler does the same thing one level up (TypeScript's compiler is written in TypeScript and
built by the previous release of itself).

MiHOS has three stages today and one planned:

| Stage | What | Needs | Produces | Where |
|---|---|---|---|---|
| 0 | A generic cross-compiler (`x86_64-elf`) | Docker | a container that can run the build, and compile freestanding code | `generic-cross-compiler/` |
| 1 | The MiHOS toolchain (`x86_64-mihos`) | the target's headers (from stage 0) | binutils, GCC, libgcc for MiHOS | `cross-compiler/` |
| 2 | MiHOS itself | the stage 1 image | `mihos.iso`, the disk images | the repository root |
| 3 | Self-hosting (planned) | the stage 1 toolchain | a GCC that runs *on* MiHOS | not yet |

## Before anything: the clone

```bash
git clone --recursive https://github.com/caioaletroca/MadeInHeavenOS.git
```

`--recursive` brings the `mlibc/` submodule (or run `git submodule update --init` afterwards).

On Windows, mlibc's ports keep their ABI headers as symlinks. Git writes them as small text
files unless symlinks are on for that submodule, which needs Windows **Developer Mode**:

```bash
git -C mlibc config core.symlinks true
```
```bash
git -C mlibc config core.autocrlf false
```
```bash
git -C mlibc checkout -- sysdeps
```

`core.autocrlf false` keeps mlibc's scripts with LF endings: they run in the Linux container.

## Stage 0: a generic cross-compiler

A *generic* cross-compiler (`x86_64-elf-gcc`) produces x86-64 ELF code for no operating system
in particular: no libc, no startup files, no idea where headers live. It is the first thing
every OS project builds (the OSDev wiki's "GCC Cross-Compiler" page), because the host's own
compiler targets the host OS and quietly brings in its headers and libraries.

What it can build: anything freestanding. A kernel, `libk.a`, even a libc and programs linked
with `-nostdlib` and explicit startup files. MiHOS's first kernels were built with it, before
the `x86_64-mihos` target existed (2022).

What it is for now: running the build far enough to install MiHOS's headers into a sysroot,
the one input stage 1 needs. That step only copies files, so today any container with `make`
would do; the generic compiler matters again whenever stage 1 needs something *compiled* for
the target before the target's compiler exists.

`generic-cross-compiler/` holds the image: `randomdude/gcc-cross-x86_64-elf` (a prebuilt
`x86_64-elf` GCC from Docker Hub) plus the tools the build uses (`xorriso` and GRUB for the
ISO, `e2fsprogs` for the disk images).

From `generic-cross-compiler/`, Windows:

```bash
docker build . -t elf-compiler
```
```bash
docker run --rm -it -v "%cd%/..:/root/env" elf-compiler
```

(`docker-build.bat` and `docker-run.bat` hold the same two commands.) Inside the container, at
`/root/env` (the repository):

```bash
make headers
```
```bash
make -C libc install-headers
```

The first installs the kernel/user contract (`kernel/include/uapi`: `<mihos/syscall.h>`, …),
the second our libc's headers. Both land in `build/x86_64-pc/sysroot/usr/include`.

## Stage 1: the MiHOS toolchain

GCC and binutils with an `x86_64-mihos` target, from two forks in the
[MiH-Operating-System](https://github.com/MiH-Operating-System) organization:

| Fork | Branch | Base | What the target patch adds |
|---|---|---|---|
| `binutils` | `mihos-2.47` | binutils 2.47 | `config.sub`, `bfd`/`gas`/`ld` target entries, `elf_x86_64_mihos` and `elf_i386_mihos` linker emulations |
| `gcc` | `mihos-16` | GCC 16.2.0 | `gcc/config/mihos.h` (link `-lc`, startup files, `__mihos__`, `_GNU_SOURCE` for C++), a `no-red-zone` multilib, libgcc's crt files, a libstdc++ entry |

Each branch is the release tarball imported as one commit, then the target patch on top, so
`git log` shows exactly what MiHOS changes. To move to a newer release: import the new tarball
on a new branch and cherry-pick the patch.

`cross-compiler/Dockerfile` builds on Debian:

1. installs the build tools (GMP, MPFR, MPC, ISL, bison, flex, texinfo, …);
2. clones both forks at their branches (`--depth 1`);
3. copies `cross-compiler/sysroot/` (the headers from stage 0) to `/root/env/sysroot`, the
   compiler's default sysroot (`--with-sysroot`);
4. builds and installs binutils (`src/build-binutils.sh`);
5. builds and installs GCC and libgcc only (`src/build-gcc.sh`): no libstdc++ yet, no gcov.
   libgcc is built twice, with and without the red zone (the kernel needs it off);
6. adds GRUB, xorriso, e2fsprogs, meson and ninja for stage 2.

Both scripts run under `set -e`: a failed step fails the image build instead of leaving an
image without libgcc.

From `cross-compiler/`, Windows:

```bash
robocopy ../build/x86_64-pc/sysroot ./sysroot /E
```
```bash
docker build . -t mihos-compiler
```

(`docker-build.bat` does both.) The GCC build takes around half an hour.

Two Docker cache rules:

- The copied sysroot comes before the GCC build, so any header that changed since the last
  image rebuilds GCC. After a change that is not about headers (a Dockerfile line after GCC,
  say), run only `docker build`.
- `git clone` lines never change text, so Docker reuses the old clone after a fork moves.
  Pick up a new commit on `mihos-16` or `mihos-2.47` with `docker build --no-cache`.

Every compile and link in the OS build passes `--sysroot=build/x86_64-pc/sysroot`, so the
copied sysroot in the image only matters while GCC itself is being built.

## Stage 2: MiHOS

From `cross-compiler/`:

```bash
docker run --rm -it -v "%cd%/..:/root/env" mihos-compiler
```

Inside, at `/root/env`:

```bash
make all
```

`make all` runs `headers → libc → kernel → mlibc → user → grub`, then the disk images
(`BUILD_RUN.md` lists every step and output). The first `make mlibc` downloads mlibc's meson
subprojects (frigg, libsmarter, the freestanding headers) into `mlibc/subprojects/`.

Then, from the repository root on the host:

```bash
./run-tests.sh
```

## What D10 changes

D10 moves user programs to mlibc, and mlibc changes how the loop closes. Its headers install
with no compiler at all (`-Dheaders_only=true`: meson only adds C and C++ when building the
library), so the order becomes mlibc's own:

```text
mlibc headers ─▶ binutils + GCC + libgcc ─▶ mlibc libc.a ─▶ libstdc++ ─▶ MiHOS
```

- Stage 1's input becomes mlibc's headers instead of our libc's.
- GCC's startup files move from our `crt0.o` to mlibc's `crt1.o`.
- libstdc++ is built after mlibc, against it.
- Stage 0 leaves MiHOS's critical path. It stays here: it is still how an OS starts before it
  has a libc that installs without a compiler.

## Stage 3: self-hosting (planned)

The stage 1 toolchain runs on Linux and produces MiHOS programs. Self-hosting builds binutils
and GCC *as* MiHOS programs (host and target both `x86_64-mihos`, built by the stage 1 toolchain:
a "Canadian cross" when build, host and target are three different systems, a plain cross build
here), then builds MiHOS on MiHOS with them. Comparing what the native compiler produces with
what the cross compiler produced is the classic check that the loop is closed.

## Status

- Stage 1: rebuilt 2026-11-15 (GCC 16.2.0, binutils 2.47, `_GNU_SOURCE` for C++).
- Stage 2: every build.
- Stage 0, and the whole chain from an empty Docker: not re-run since the toolchain moved to the
  organization forks (2026-11-14). The steps above follow the current files.
