@echo off
rem Boots mihos.iso in QEMU with the ext2 test disk (both built by make all,
rem in build\x86_64-pc\images) as the IDE primary master, hda, like
rem run-tests.sh; the CD stays the secondary master; part.img (the partitioned
rem disk) the primary slave, hdb, when it is there. -snapshot: the run never
rem writes to the disks. Without disk.img, the CD boots alone (the kernel then
rem logs "ext2: no disk to mount").
set images=build\x86_64-pc\images
set part=
if exist %images%\part.img set part=-drive file=%images%\part.img,format=raw,if=ide,index=1,media=disk
if exist %images%\disk.img (
    qemu-system-x86_64 -cdrom %images%\mihos.iso -drive file=%images%\disk.img,format=raw,if=ide,index=0,media=disk %part% -snapshot
) else (
    qemu-system-x86_64 -cdrom %images%\mihos.iso
)
