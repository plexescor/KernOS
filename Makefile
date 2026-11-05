# Toolchain
CXX     := g++
AS      := nasm
LD      := ld
OBJCOPY := objcopy

# Flags
CXXFLAGS := \
    -ffreestanding -fno-stack-protector -fno-stack-check \
    -fno-lto -fno-PIC -fno-PIE \
    -fno-exceptions -fno-rtti \
    -m64 -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -mcmodel=kernel \
    -std=c++23 -Wall -Wextra -O2 \
    -Iinclude -Ilimine

ASFLAGS := -f elf64

LDFLAGS := \
    -nostdlib -static -m elf_x86_64 \
    -z max-page-size=0x1000 \
    -no-pie \
    -T linker.ld

# Sources & objects
SRCS_CXX := src/kernel/kernel.cpp \
             src/framebuffer/framebuffer.cpp \
             src/TTY/TTY.cpp \
			 src/GDT/GDT.cpp \
			 src/IDT/IDT.cpp
SRCS_ASM := src/boot/boot.asm \
			 src/GDT/GDT.asm \
			 src/IDT/IDT.asm

OBJS := build/kernel/kernel.o \
        build/framebuffer/framebuffer.o \
        build/TTY/TTY.o \
        build/boot/boot.asm.o \
		build/GDT/GDT.o \
		build/GDT/GDT.asm.o \
		build/IDT/IDT.o \
        build/IDT/IDT.asm.o \
        fonts/font.o

# Targets
.PHONY: all iso run clean

all: build/kernel.elf

# Link
build/kernel.elf: $(OBJS) linker.ld | build
	$(LD) $(LDFLAGS) $(OBJS) -o $@

# Compile C++
build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Embed PSF font
fonts/font.o: fonts/font.psf
	$(OBJCOPY) -I binary -O elf64-x86-64 -B i386:x86-64 $< $@

# Assemble
build/boot/boot.asm.o: src/boot/boot.asm | build
	$(AS) $(ASFLAGS) $< -o $@

# Assemble
build/GDT/GDT.asm.o: src/GDT/GDT.asm | build
	$(AS) $(ASFLAGS) $< -o $@

# Assemble
build/IDT/IDT.asm.o: src/IDT/IDT.asm | build
	$(AS) $(ASFLAGS) $< -o $@

build:
	mkdir -p $(dir $(OBJS))

# ISO
iso: build/kernel.elf
	mkdir -p isoRoot/boot/limine
	mkdir -p isoRoot/EFI/BOOT
	cp build/kernel.elf             isoRoot/boot/kernel.elf
	cp limine.conf                  isoRoot/boot/limine/limine.conf
	cp limine/limine-bios.sys       isoRoot/boot/limine/
	cp limine/limine-bios-cd.bin    isoRoot/boot/limine/
	cp limine/limine-uefi-cd.bin    isoRoot/boot/limine/
	cp limine/BOOTX64.EFI           isoRoot/EFI/BOOT/BOOTX64.EFI
	cp limine/BOOTIA32.EFI          isoRoot/EFI/BOOT/BOOTIA32.EFI
	xorriso -as mkisofs \
	    -b boot/limine/limine-bios-cd.bin \
	    -no-emul-boot -boot-load-size 4 -boot-info-table \
	    --efi-boot boot/limine/limine-uefi-cd.bin \
	    -efi-boot-part --efi-boot-image --protective-msdos-label \
	    isoRoot -o TantrumsOS.iso
	./limine/limine bios-install TantrumsOS.iso

run: iso
	cp /usr/share/edk2/x64/OVMF_VARS.4m.fd ./MY_VARS.fd
	qemu-system-x86_64 \
	    -machine q35 \
	    -drive if=pflash,format=raw,readonly=on,file=/usr/share/edk2/x64/OVMF_CODE.4m.fd \
	    -drive if=pflash,format=raw,file=./MY_VARS.fd \
	    -cdrom TantrumsOS.iso \
	    -m 1G \
	    -monitor stdio

clean:
	rm -rf build isoRoot TantrumsOS.iso