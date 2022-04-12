all: $(libc) 
	$(MAKE) -C kernel
	$(MAKE) -C libc

clean:
	$(MAKE) -C kernel clean
	$(MAKE) -C libc clean
	rm -rf isodir
	rm -rf sysroot

purge:
	$(MAKE) -C kernel purge
	$(MAKE) -C libc purge