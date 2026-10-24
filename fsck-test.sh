#!/usr/bin/env bash
#
# Runs the tests on a copy of the test disk that the run may write (no
# -snapshot), then checks the copy with e2fsck in the mihos-compiler image.
# Exits 0 only if the tests passed, e2fsck found nothing to fix (the kernel
# left a consistent ext2 behind: bitmaps, free counts, i_blocks, block maps)
# and the volume says clean (the halt marked it so: while mounted it does not).
#
#   ./fsck-test.sh [iso [disk]]   defaults: mihos.iso, disk.img
#
# The copy is build/fsck.img, kept for a look (e.g. debugfs) after a failure.
# Environment: IMAGE (default mihos-compiler); QEMU and TIMEOUT go to run-tests.sh.

set -u

iso=${1:-mihos.iso}
disk=${2:-disk.img}
image=${IMAGE:-mihos-compiler}
copy=build/fsck.img

if [ ! -f "$disk" ]; then
    echo "fsck-test: $disk not found (build it first: make all, in Docker)" >&2
    exit 2
fi

mkdir -p build
cp "$disk" "$copy" || exit 2

SNAPSHOT=0 ./run-tests.sh "$iso" "$copy" || {
    echo "fsck-test: FAIL (the tests failed; e2fsck not run)"
    exit 1
}

# Docker on Windows wants a Windows path for the volume, and Git Bash must not rewrite /disk
build_dir=$PWD/build
if command -v cygpath >/dev/null 2>&1; then
    build_dir=$(cygpath -m "$build_dir")
fi

if ! command -v docker >/dev/null 2>&1; then
    echo "fsck-test: docker not found; check the copy by hand:" >&2
    echo "  docker run --rm -v \"$build_dir:/disk\" $image e2fsck -fn /disk/fsck.img" >&2
    exit 2
fi

# -f: check even though the file system says it is clean; -n: change nothing, answer no
MSYS_NO_PATHCONV=1 docker run --rm -v "$build_dir:/disk" "$image" e2fsck -fn /disk/fsck.img
status=$?

# e2fsck's exit status: 0 clean; 4 errors left uncorrected (with -n, any error); 8 and up: it could not run
case $status in
0) ;;
4) echo "fsck-test: FAIL (e2fsck found errors in $copy)"; exit 1 ;;
*) echo "fsck-test: e2fsck could not check $copy (status $status)" >&2; exit 2 ;;
esac

# The mount cleared s_state's clean bit (the self-test checks that); only the halt sets it again
state=$(MSYS_NO_PATHCONV=1 docker run --rm -v "$build_dir:/disk" "$image" dumpe2fs -h /disk/fsck.img 2>/dev/null |
    sed -n 's/^Filesystem state: *//p')
if [ "$state" != "clean" ]; then
    echo "fsck-test: FAIL (the volume says '$state' after the halt, not clean)"
    exit 1
fi

echo "fsck-test: ok (tests passed, e2fsck found nothing to fix, the halt left the volume clean)"
