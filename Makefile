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
    -Iinclude -Ilimine -Iinclude_Drivers

ASFLAGS := -f elf64

LDFLAGS := \
    -nostdlib -static -m elf_x86_64 \
    -z max-page-size=0x1000 \
    -no-pie \
    -T linker.ld

# Sources & objects
SRCS_CXX := src/Kernel/Kernel.cpp \
             src/Framebuffer/Framebuffer.cpp \
             src/TTY/TTY.cpp \
			 src/GDT/GDT.cpp \
			 src/IDT/IDT.cpp \
			 src/IO/PortIO.cpp \
			 src/IO/Keyboard.cpp \
			 src/IO/PCI.cpp \
			 src/CPU/APIC.cpp \
			 src/CPU/MSR.cpp \
			 src/CPU/ACPI.cpp \
			 src/Memory/PhysicalMemoryManager.cpp \
			 src/Memory/VirtualMemoryManager.cpp \
			 src/Libc/Memory.cpp \
			 drivers/IO/NVMe.cpp
SRCS_ASM := src/Boot/Boot.asm \
			 src/GDT/GDT.asm \
			 src/IDT/IDT.asm

OBJS := build/Kernel/Kernel.o \
        build/Framebuffer/Framebuffer.o \
        build/TTY/TTY.o \
        build/Boot/Boot.asm.o \
		build/GDT/GDT.o \
		build/GDT/GDT.asm.o \
		build/IDT/IDT.o \
        build/IDT/IDT.asm.o \
        build/IO/PortIO.o \
        build/IO/Keyboard.o \
        build/IO/PCI.o \
        build/CPU/APIC.o \
        build/CPU/MSR.o \
        build/CPU/ACPI.o \
        build/Memory/PhysicalMemoryManager.o \
        build/Memory/VirtualMemoryManager.o \
        build/Libc/Memory.o \
        Fonts/Font.o \
        build/drivers/IO/NVMe.o

# Targets
.PHONY: all iso run clean

all: build/Kernel.elf

# Link
build/Kernel.elf: $(OBJS) linker.ld | build
	$(LD) $(LDFLAGS) $(OBJS) -o $@

# Compile C++
build/%.o: src/%.cpp | build
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/drivers/%.o: drivers/%.cpp | build
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Embed PSF font
Fonts/Font.o: Fonts/Font.psf
	$(OBJCOPY) -I binary -O elf64-x86-64 -B i386:x86-64 $< $@

# Assemble
build/Boot/Boot.asm.o: src/Boot/Boot.asm | build
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
iso: build/Kernel.elf
	mkdir -p isoRoot/boot/limine
	mkdir -p isoRoot/EFI/BOOT
	cp build/Kernel.elf             isoRoot/boot/Kernel.elf
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
	    isoRoot -o KernOS.iso
	./limine/limine bios-install KernOS.iso

run: iso
	cp /usr/share/edk2/x64/OVMF_VARS.4m.fd ./MY_VARS.fd
	qemu-system-x86_64 \
	    -machine q35 \
	    -drive if=pflash,format=raw,readonly=on,file=/usr/share/edk2/x64/OVMF_CODE.4m.fd \
	    -drive if=pflash,format=raw,file=./MY_VARS.fd \
	    -cdrom KernOS.iso \
	    -drive file=nvme_disk.img,if=none,id=nvme0 \
	    -device nvme,drive=nvme0,serial=kernos0 \
	    -m 2G \
	    -monitor stdio \
	    -display gtk,grab-on-hover=on

clean:
	rm -rf build isoRoot KernOS.iso
