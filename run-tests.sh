#!/usr/bin/env bash
#
# Boots the ISO headless in QEMU (GRUB's default entry: the automatic tests),
# captures the 0xE9 debug console and checks the result, then checks the disk
# the run left behind with e2fsck. Exits 0 only if the run powered off by
# itself after "tests: N passed, 0 failed" and the disk is consistent.
#
#   ./run-tests.sh [iso [disk [partitions]]]   defaults: mihos.iso, disk.img, part.img
#
# The disk (an ext2 image, make all builds it) is the IDE primary master, the
# partition test disk (an MBR, make all builds it too) the primary slave, the
# CD stays the secondary master. The partition disk is never changed
# (snapshot=on). The run writes to a fresh copy of it,
# build/test-disk.img (kept for a look, e.g. debugfs, after a failure): the
# image itself is never changed.
#
# The referee: e2fsck -fn on the copy (bitmaps, free counts, i_blocks, block
# maps, directories, link counts) and dumpe2fs's state (the halt marks the
# volume clean; while mounted it is not), in the mihos-compiler Docker image.
#
# Environment: QEMU (default qemu-system-x86_64), TIMEOUT in seconds (default
# 120), IMAGE (default mihos-compiler), FSCK: auto (default: check when Docker
# and the image are there, else say it was skipped), 1 (the check is required),
# 0 (no check). The log stays in run-tests.log.

set -u

iso=${1:-mihos.iso}
disk=${2:-disk.img}
part=${3:-part.img}
qemu=${QEMU:-qemu-system-x86_64}
limit=${TIMEOUT:-120}
image=${IMAGE:-mihos-compiler}
fsck=${FSCK:-auto}
log=run-tests.log
copy=build/test-disk.img

if [ ! -f "$iso" ]; then
    echo "run-tests: $iso not found (build it first: make all, in Docker)" >&2
    exit 2
fi

for image_file in "$disk" "$part"; do
    if [ ! -f "$image_file" ]; then
        echo "run-tests: $image_file not found (build it first: make all, in Docker)" >&2
        exit 2
    fi
done

if ! command -v "$qemu" >/dev/null 2>&1; then
    echo "run-tests: $qemu not found" >&2
    exit 2
fi

# Decided before the run, so a required check that cannot happen costs no run
case $fsck in
0) ;;
auto | 1)
    if ! command -v docker >/dev/null 2>&1 || ! docker image inspect "$image" >/dev/null 2>&1; then
        if [ "$fsck" = 1 ]; then
            echo "run-tests: FSCK=1 but docker or the $image image is not there" >&2
            exit 2
        fi
        fsck=skipped
    fi
    ;;
*)
    echo "run-tests: FSCK must be auto, 1 or 0" >&2
    exit 2
    ;;
esac

mkdir -p build
cp "$disk" "$copy" || exit 2

# A Windows QEMU or Docker needs Windows paths, and Git Bash must not rewrite them
log_arg=$log
disk_arg=$copy
part_arg=$part
build_dir=$PWD/build
if command -v cygpath >/dev/null 2>&1; then
    log_arg=$(cygpath -m "$PWD/$log")
    disk_arg=$(cygpath -m "$PWD/$copy")
    part_arg=$(cygpath -m "$PWD/$part")
    build_dir=$(cygpath -m "$build_dir")
fi

rm -f "$log"
start=$(date +%s)
MSYS_NO_PATHCONV=1 timeout "$limit" "$qemu" -cdrom "$iso" -m 1024 -no-reboot \
    -drive "file=$disk_arg,format=raw,if=ide,index=0,media=disk"     -drive "file=$part_arg,format=raw,if=ide,index=1,media=disk,snapshot=on" \
    -display none -debugcon "file:$log_arg"
qemu_status=$?
elapsed=$(($(date +%s) - start))

fail() {
    echo "run-tests: FAIL ($1, ${elapsed}s)"
    if [ -f "$log" ]; then
        echo "--- $log ---"
        cat "$log"
    fi
    exit 1
}

[ -f "$log" ] || fail "no debug output"

if [ "$qemu_status" -eq 124 ]; then
    fail "still running after ${limit}s: hung, or halted without powering off"
fi
[ "$qemu_status" -eq 0 ] || fail "qemu exited with status $qemu_status"

# A run that cut short shows as a missing line, not a FAIL line
grep -q '^self-tests: ok' "$log" || fail "kernel self-tests did not finish"
summary=$(grep -E '^tests: [0-9]+ passed, [0-9]+ failed' "$log" | tail -n 1)
[ -n "$summary" ] || fail "the test runner did not finish"
grep -q '^init: /usr/tests/run exited with status 0' "$log" || fail "the test runner failed"
grep -q '^system halted' "$log" || fail "no clean halt"

# Test programs print FAIL lines, also those nobody waits for (the adopted orphan)
if grep -q 'FAIL' "$log"; then
    fail "FAIL lines in the output"
fi

case "$summary" in
*", 0 failed") ;;
*) fail "$summary" ;;
esac

if [ "$fsck" = 0 ] || [ "$fsck" = skipped ]; then
    note="e2fsck not run"
    [ "$fsck" = skipped ] && note="e2fsck skipped: no docker or $image image"
    echo "run-tests: ok ($summary, $note, ${elapsed}s)"
    exit 0
fi

# -f: check even though the file system says it is clean; -n: change nothing, answer no.
# Exit status: 0 clean; 4 errors left uncorrected (with -n, any error); 8 and up: it could not run
report=$(MSYS_NO_PATHCONV=1 docker run --rm -v "$build_dir:/disk" "$image" e2fsck -fn /disk/test-disk.img 2>&1)
status=$?
case $status in
0) ;;
4)
    echo "run-tests: FAIL (the tests passed, but e2fsck found errors in $copy, ${elapsed}s)"
    echo "$report"
    exit 1
    ;;
*)
    echo "run-tests: e2fsck could not check $copy (status $status)" >&2
    echo "$report" >&2
    exit 2
    ;;
esac

# The mount cleared s_state's clean bit (the self-test checks that); only the halt sets it again
state=$(MSYS_NO_PATHCONV=1 docker run --rm -v "$build_dir:/disk" "$image" dumpe2fs -h /disk/test-disk.img 2>/dev/null |
    sed -n 's/^Filesystem state: *//p')
if [ "$state" != "clean" ]; then
    echo "run-tests: FAIL (the volume says '$state' after the halt, not clean, ${elapsed}s)"
    exit 1
fi

echo "run-tests: ok ($summary, e2fsck clean, ${elapsed}s)"
