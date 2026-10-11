#!/usr/bin/env bash
#
# Boots the ISO headless in QEMU (GRUB's default entry: the automatic tests),
# captures the 0xE9 debug console and checks the result. Exits 0 only if the
# run powered off by itself after "tests: N passed, 0 failed".
#
#   ./run-tests.sh [iso]        default: mihos.iso
#
# Environment: QEMU (default qemu-system-x86_64), TIMEOUT in seconds (default
# 120). The log stays in run-tests.log.

set -u

iso=${1:-mihos.iso}
qemu=${QEMU:-qemu-system-x86_64}
limit=${TIMEOUT:-120}
log=run-tests.log

if [ ! -f "$iso" ]; then
    echo "run-tests: $iso not found (build it first: make all, in Docker)" >&2
    exit 2
fi

if ! command -v "$qemu" >/dev/null 2>&1; then
    echo "run-tests: $qemu not found" >&2
    exit 2
fi

# A Windows QEMU needs a Windows path for -debugcon, and Git Bash must not rewrite it
log_arg=$log
if command -v cygpath >/dev/null 2>&1; then
    log_arg=$(cygpath -m "$PWD/$log")
fi

rm -f "$log"
start=$(date +%s)
MSYS_NO_PATHCONV=1 timeout "$limit" "$qemu" -cdrom "$iso" -m 1024 -no-reboot \
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

echo "run-tests: ok ($summary, ${elapsed}s)"
