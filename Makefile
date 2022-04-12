all: $(libk)
	$(MAKE) -C libc
	$(MAKE) -C kernel
	$(MAKE) -C grub

clean:
	$(MAKE) -C libc clean
	$(MAKE) -C kernel clean
	$(MAKE) -C grub clean
	rm -rf isodir
	rm -rf sysroot

purge:
	$(MAKE) -C libc purge
	$(MAKE) -C kernel purge
	$(MAKE) -C grub purge