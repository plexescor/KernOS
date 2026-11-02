ASM = nasm
CXX = g++
LD = ld

ASMFLAGS = -f elf32
CXXFLAGS = -m32 -ffreestanding -fno-exceptions -fno-rtti -nostdlib -std=c++17
LDFLAGS = -m elf_i386 -T linker.ld

BUILD = build

all: TantrumsOS.iso

$(BUILD)/boot.o: boot/boot.asm
	mkdir -p $(BUILD)
	$(ASM) $(ASMFLAGS) boot/boot.asm -o $(BUILD)/boot.o

$(BUILD)/kernel.o: kernel/kernel.cpp
	mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -c kernel/kernel.cpp -o $(BUILD)/kernel.o

$(BUILD)/kernel.bin: $(BUILD)/boot.o $(BUILD)/kernel.o
	$(LD) $(LDFLAGS) -o $(BUILD)/kernel.bin $(BUILD)/boot.o $(BUILD)/kernel.o

iso/TantrumsOS.iso: $(BUILD)/kernel.bin
	mkdir -p iso/boot/grub
	cp $(BUILD)/kernel.bin iso/boot/
	echo 'set timeout=0' > iso/boot/grub/grub.cfg
	echo 'set default=0' >> iso/boot/grub/grub.cfg
	echo 'menuentry "TantrumsOS" { multiboot /boot/kernel.bin }' >> iso/boot/grub/grub.cfg
	grub-mkrescue -o iso/TantrumsOS.iso iso

run: iso/TantrumsOS.iso
	qemu-system-x86_64 -cdrom iso/TantrumsOS.iso

clean:
	rm -rf $(BUILD) iso

.PHONY: all run clean