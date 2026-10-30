#!/usr/bin/env bash
#
# Boots the ISO headless in QEMU twice: live (the initrd is /, GRUB entry
# "tests") and from the disk (/ is part.img's partition 1, entry "tests-disk").
# Each run captures the 0xE9 debug console and is checked, then the disks it
# left behind are checked with e2fsck. Exits 0 only if both runs powered off
# by themselves after "tests: N passed, 0 failed" and every file system is
# consistent and clean.
#
#   ./run-tests.sh [iso [disk [partitions]]]
#
# Defaults: mihos.iso, disk.img and part.img in build/<target>/images/ (make
# all builds them; TARGET, default x86_64-pc). The logs and the disk copies go
# to build/<target>/test/: run-tests-live.log, run-tests-disk.log.
#
# The entry: QEMU sets the SMBIOS product name to mihos-entry:<id>, and
# grub.cfg boots that entry at once (no menu).
#
# The disk (an ext2 image) is the IDE primary master, the partitioned disk (an
# MBR, the installed system on partition 1) the primary slave, the CD stays
# the secondary master. Each run writes to fresh copies, test/test-disk.img
# and test/test-part.img (kept for a look, e.g. debugfs, after a failure): the
# images themselves are never changed.
#
# The referee: e2fsck -fn on the disk copy and on the partition copy's root
# file system (copied out of it with dd: bitmaps, free counts, i_blocks, block
# maps, directories, link counts) and dumpe2fs's state (a halt marks a mounted
# volume clean; while mounted it is not), in the mihos-compiler Docker image.
#
# Environment: QEMU (default qemu-system-x86_64), TIMEOUT in seconds per run
# (default 120), IMAGE (default mihos-compiler), FSCK: auto (default: check
# when Docker and the image are there, else say it was skipped), 1 (the check
# is required), 0 (no check).

set -u

target=${TARGET:-x86_64-pc}
images=build/$target/images
test_dir=build/$target/test
iso=${1:-$images/mihos.iso}
disk=${2:-$images/disk.img}
part=${3:-$images/part.img}
qemu=${QEMU:-qemu-system-x86_64}
limit=${TIMEOUT:-120}
image=${IMAGE:-mihos-compiler}
fsck=${FSCK:-auto}
disk_copy=$test_dir/test-disk.img
part_copy=$test_dir/test-part.img
part1_dd="skip=1 count=12" # partition 1 in MiB (disk-tools/partition-disk.sh): sector 2048, 24576 sectors

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

# Decided before the runs, so a required check that cannot happen costs no run
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

mkdir -p "$test_dir"

# A Windows QEMU or Docker needs Windows paths, and Git Bash must not rewrite them
path_arg() {
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -m "$PWD/$1"
    else
        echo "$1"
    fi
}
test_mount=$(path_arg "$test_dir")

# e2fsck and dumpe2fs on one file system: $1 the file in test/, $2 what to call
# it, $3 (a partition) dd's skip= and count= in MiB: that part is copied out
# first (dumpe2fs takes "file?offset=N" for a LABEL=-style tag). Exits on a failure.
check_fs() {
    local source="/disk/$1" prepare="" report status state
    if [ $# -ge 3 ]; then
        prepare="dd if=/disk/$1 of=/tmp/fs bs=1M $3 2>/dev/null && "
        source=/tmp/fs
    fi

    # -f: check even though the file system says it is clean; -n: change nothing, answer no.
    # Exit status: 0 clean; 4 errors left uncorrected (with -n, any error); 8 and up: it could not run
    report=$(MSYS_NO_PATHCONV=1 docker run --rm -v "$test_mount:/disk" "$image" sh -c "${prepare}e2fsck -fn $source" 2>&1)
    status=$?
    case $status in
    0) ;;
    4)
        echo "run-tests: FAIL ($name: the tests passed, but e2fsck found errors in $2, ${elapsed}s)"
        echo "$report"
        exit 1
        ;;
    *)
        echo "run-tests: e2fsck could not check $2 (status $status)" >&2
        echo "$report" >&2
        exit 2
        ;;
    esac

    # A writable mount clears s_state's clean bit (the self-test checks that); only the halt sets it again
    state=$(MSYS_NO_PATHCONV=1 docker run --rm -v "$test_mount:/disk" "$image" sh -c "${prepare}dumpe2fs -h $source" 2>/dev/null |
        sed -n 's/^Filesystem state: *//p')
    if [ "$state" != "clean" ]; then
        echo "run-tests: FAIL ($name: $2 says '$state' after the halt, not clean, ${elapsed}s)"
        exit 1
    fi
}

fail() {
    echo "run-tests: FAIL ($name: $1, ${elapsed}s)"
    if [ -f "$log" ]; then
        echo "--- $log ---"
        cat "$log"
    fi
    exit 1
}

# One boot: $1 its name (the log's), $2 the GRUB entry's id. Exits on a failure;
# sets $result to its summary on success
run_boot() {
    name=$1
    local entry=$2
    log=$test_dir/run-tests-$name.log

    cp "$disk" "$disk_copy" || exit 2
    cp "$part" "$part_copy" || exit 2
    rm -f "$log"

    local start qemu_status summary
    start=$(date +%s)
    MSYS_NO_PATHCONV=1 timeout "$limit" "$qemu" -cdrom "$iso" -m 1024 -no-reboot \
        -smbios "type=1,product=mihos-entry:$entry" \
        -drive "file=$(path_arg "$disk_copy"),format=raw,if=ide,index=0,media=disk" \
        -drive "file=$(path_arg "$part_copy"),format=raw,if=ide,index=1,media=disk" \
        -display none -debugcon "file:$(path_arg "$log")"
    qemu_status=$?
    elapsed=$(($(date +%s) - start))

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
        local note="e2fsck not run"
        [ "$fsck" = skipped ] && note="e2fsck skipped: no docker or $image image"
        result="$summary, $note, ${elapsed}s"
        return
    fi

    check_fs test-disk.img "$disk_copy"
    check_fs test-part.img "$part_copy's partition 1" "$part1_dd"
    result="$summary, e2fsck clean, ${elapsed}s"
}

run_boot live tests
live=$result
run_boot disk tests-disk
echo "run-tests: ok (live: $live; from disk: $result)"
