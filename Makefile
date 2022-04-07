# Lists of files to compile
kernel_source_files := $(shell find kernel/kernel -name *.c)
kernel_object_files := $(patsubst kernel/kernel/%.c, build/kernel/%.o, $(kernel_source_files))
# x86_64_c_source_files := $(shell find libc -name *.c)
# x86_64_c_object_files := $(patsubst libc/%.c, build/x86_64/%.o, $(x86_64_c_source_files))
x86_64_asm_source_files := $(shell find kernel/arch/x86_64 -name *.asm)
x86_64_asm_object_files := $(patsubst kernel/arch/x86_64/%.asm, build/x86_64/%.o, $(x86_64_asm_source_files))

x86_64_object_files := $(x86_64_c_object_files) $(x86_64_asm_object_files)

$(kernel_object_files): build/kernel/%.o : kernel/kernel/%.c
	mkdir -p $(dir $@) && \
	x86_64-elf-gcc -c -I src/intf -ffreestanding $(patsubst build/kernel/%.o, kernel/kernel/%.c, $@) -o $@

# $(x86_64_c_object_files): build/x86_64/%.o : libc/%.c
# 	mkdir -p $(dir $@) && \
# 	x86_64-elf-gcc -c -I src/intf -ffreestanding $(patsubst build/x86_64/%.o, libc/%.c, $@) -o $@

$(x86_64_asm_object_files): build/x86_64/%.o : kernel/arch/x86_64/%.asm
	mkdir -p $(dir $@) && \
	nasm -f elf64 $(patsubst build/x86_64/%.o, kernel/arch/x86_64/%.asm, $@) -o $@

.PHONY: run build
run:
	qemu-system-x86_64 -cdrom mihos.iso

build: $(kernel_object_files) $(x86_64_object_files)
	mkdir -p dist/x86_64 && \
	ld -n -o dist/x86_64/mihos.kernel -T kernel/arch/x86_64/linker.ld $(kernel_object_files) $(x86_64_object_files) && \
	mkdir -p dist/x86_64/iso/boot/grub && \
	cp grub/grub.cfg dist/x86_64/iso/boot/grub
	cp dist/x86_64/mihos.kernel dist/x86_64/iso/boot
	grub-mkrescue /usr/lib/grub/i386-pc -o mihos.iso dist/x86_64/iso
