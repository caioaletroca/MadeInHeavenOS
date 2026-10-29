#!/bin/sh
#
# Writes the partitioned disk's empty layout: 16 MiB (32768 sectors) with an
# MBR. The Makefile then puts an ext2 file system on partition 1, the system
# as an installer would leave it (the root for the "from disk" boot); the
# other slots are for the partition self-test, with markers where partition 4
# starts and ends, so a read at the wrong offset or past it shows:
#
#   slot 1  Linux (0x83)     sectors 2048..26623   (24576)  the root file system
#   slot 2  empty (0x00)
#   slot 3  extended (0x05)  sectors 26624..28671           skipped (no logical partitions yet)
#   slot 4  Linux (0x83)     sectors 28672..30718  (2047)   "mihos-part4 first/last"
#
# Sectors 30719..32767 are in no partition. Made with printf and dd only (the
# build image has no sfdisk): octal escapes, since dash's printf has no \x.
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
dd if=/dev/zero of="$image" bs=512 count=32768 status=none

entry 1 131 2048 24576 # 0x83
entry 3 5 26624 2048   # 0x05
entry 4 131 28672 2047
printf '\125\252' | put 510 # the 0x55 0xAA signature

printf 'mihos-part4 first' | put $((28672 * 512))
printf 'mihos-part4 last' | put $((30718 * 512))
