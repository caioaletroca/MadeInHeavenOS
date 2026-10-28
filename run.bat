@echo off
rem Boots mihos.iso in QEMU with the ext2 test disk (both built by make all) as
rem the IDE primary master, hda, like run-tests.sh; the CD stays the secondary
rem master. -snapshot: the run never writes to disk.img. Without disk.img, the
rem CD boots alone (the kernel then logs "ext2: no disk to mount").
if exist disk.img (
    qemu-system-x86_64 -cdrom mihos.iso -drive file=disk.img,format=raw,if=ide,index=0,media=disk -snapshot
) else (
    qemu-system-x86_64 -cdrom mihos.iso
)
