@echo off
rem Boots mihos.iso in QEMU with the ext2 test disk (both built by make all) as
rem the IDE primary master, hda, like run-tests.sh; the CD stays the secondary
rem master; part.img (the partition test disk) the primary slave, hdb, when it
rem is there. -snapshot: the run never writes to the disks. Without disk.img, the
rem CD boots alone (the kernel then logs "ext2: no disk to mount").
set part=
if exist part.img set part=-drive file=part.img,format=raw,if=ide,index=1,media=disk
if exist disk.img (
    qemu-system-x86_64 -cdrom mihos.iso -drive file=disk.img,format=raw,if=ide,index=0,media=disk %part% -snapshot
) else (
    qemu-system-x86_64 -cdrom mihos.iso
)
