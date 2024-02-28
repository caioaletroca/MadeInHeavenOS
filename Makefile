all: $(libk)
	$(MAKE) -C libc
	$(MAKE) -C kernel
	$(MAKE) -C grub

install:
	$(MAKE) -C libc install
	$(MAKE) -C kernel install

clean:
	$(MAKE) -C libc clean
	$(MAKE) -C kernel clean
	$(MAKE) -C grub clean
	rm -rf isodir
	rm -rf sysroot
	rm -rf mihos.iso

purge:
	$(MAKE) -C libc purge
	$(MAKE) -C kernel purge
	$(MAKE) -C grub purge

dump:
	objdump -d kernel/build/x86_64/kernel > mihos.txt