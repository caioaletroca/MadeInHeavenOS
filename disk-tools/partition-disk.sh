#!/bin/sh
#
# Writes the partition test disk: 4 MiB (8192 sectors) with an MBR, read by
# the partition self-test. No file systems, only markers where each partition
# starts and ends, so a read at the wrong offset or past a partition shows:
#
#   slot 1  Linux (0x83)     sectors 2048..4095  (2048)  "mihos-part1 first/last"
#   slot 2  empty (0x00)
#   slot 3  extended (0x05)  sectors 4096..6143          skipped (no logical partitions yet)
#   slot 4  Linux (0x83)     sectors 6144..8190  (2047)  "mihos-part4 first/last"
#
# Sector 8191 is in no partition. Made with printf and dd only (the build
# image has no sfdisk): octal escapes, since dash's printf has no \x.
#
#   disk-tools/partition-disk.sh part.img

set -eu

image=$1

# Bytes from stdin written at a byte offset, the rest of the image untouched
put() {
    dd of="$image" bs=1 seek="$1" conv=notrunc status=none
}

# A 32-bit little-endian number as 4 raw bytes
le32() {
    printf "$(printf '\\%03o\\%03o\\%03o\\%03o' $(($1 & 255)) $(($1 >> 8 & 255)) $(($1 >> 16 & 255)) $(($1 >> 24 & 255)))"
}

# entry slot type start count: a 16-byte entry at 446 + 16 * (slot - 1):
# status and CHS start (unused: 0), type, CHS end (0), first LBA, sector count
entry() {
    {
        printf '\000\000\000\000'
        printf "\\$(printf '%03o' "$2")"
        printf '\000\000\000'
        le32 "$3"
        le32 "$4"
    } | put $((446 + 16 * ($1 - 1)))
}

rm -f "$image"
dd if=/dev/zero of="$image" bs=512 count=8192 status=none

entry 1 131 2048 2048 # 0x83
entry 3 5 4096 2048   # 0x05
entry 4 131 6144 2047
printf '\125\252' | put 510 # the 0x55 0xAA signature

printf 'mihos-part1 first' | put $((2048 * 512))
printf 'mihos-part1 last' | put $((4095 * 512))
printf 'mihos-part4 first' | put $((6144 * 512))
printf 'mihos-part4 last' | put $((8190 * 512))
