# Lists of files to compile
x86_64_asm_source_files := $(shell find kernel/arch/x86_64 -name *.asm)
x86_64_asm_object_files := $(patsubst kernel/arch/x86_64/%.asm, build/x86_64/%.o, $(x86_64_asm_source_files))

$(x86_64_asm_object_files): build/x86_64/%.o : kernel/arch/x86_64/%.asm
	mkdir -p $(dir $@) && \
	nasm -f elf64 $(patsubst build/x86_64/%.o, kernel/arch/x86_64/%.asm, $@) -o $@

.PHONY: run build
run:
	qemu-system-x86_64 -cdrom dist/x86_64/kernel.iso

build: $(x86_64_asm_object_files)
	mkdir -p dist/x86_64 && \
	ld -n -o dist/x86_64/kernel.bin -T kernel/arch/x86_64/linker.ld $(x86_64_asm_object_files) && \
	mkdir -p dist/x86_64/iso/boot/grub && \
	cp kernel/arch/x86_64/grub.cfg dist/x86_64/iso/boot/grub
	cp dist/x86_64/kernel.bin dist/x86_64/iso/boot
	grub-mkrescue /usr/lib/grub/i386-pc -o dist/x86_64/kernel.iso dist/x86_64/iso
