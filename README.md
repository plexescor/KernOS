# KernOS

> I want this OS to be daily drivable

A from-scratch x86-64 operating system written in C++23 and NASM. No Linux base, no existing kernel. Built entirely from bare metal up.

The goal is a modern, clean-slate OS with no legacy baggage. No 40 years of POSIX duct tape, no backwards compatibility constraints from 1978. Every design decision is made knowing what we know in 2026.

## Status

Early kernel development. Currently boots via Limine on UEFI, runs in QEMU, and accepts keyboard input(Doesn't print anything..yet).

**Working:**
> [!NOTE]
> Working does not mean it doesnt have any bugs or other bad things, also typing doesnt work on Bare Metal Real Hardware
- GDT with all descriptor entries including user-mode segments
- IDT with handlers for all standard x86-64 exceptions
- ISR stubs in NASM with correct symbol exports and stack frame setup
- LAPIC initialization with legacy 8259 PIC disabled
- IO APIC with keyboard IRQ routing to vector 0x21
- PS/2 keyboard driver with scancode translation, shift and capslock state tracking
- PSF2 bitmap font renderer on Limine framebuffer
- TTY with global cursor state and newline handling
- Physical memory manager using a bitmap allocator over Limine's memory map
- Virtual memory manager and 4-level page tables
- Port I/O (inb and outb currently)
- MSR read and write
- Kernel heap
- ACPI parsing (Keyboard done but no typing on bare metal)

**In progress:**
- ACPI parsing (Keyboard)
- PCI enumeration
- NVMe driver (just started 0% progress)

**Not started yet:**
- Filesystem
- Userspace, ELF loader, syscalls
- Scheduler

## Requirements

- x86-64 CPU (64 Bit)
- UEFI firmware
- LAPIC support
- ACPI Revision 2.0
- 2GB RAM (QEMU default for testing)

These are the minimum tested requirements. 32-bit and legacy BIOS are not supported and will not be.

## Building

The build system is still changing frequently. Proper build instructions will be added once it stabilizes. In the meantime, the project requires a cross-compiler targeting x86-64 ELF, NASM, and OVMF firmware for UEFI emulation in QEMU.

## Planned

The roadmap roughly in order:

- ACPI parsing and PCI enumeration
- NVMe driver and ext2 filesystem
- Userspace, ELF loader, syscalls, scheduler
- libc and shell
- WiFi driver for RTL8852BE
- GPU driver for Radeon 860M (RDNA 3.5) with Vulkan implementation
- Compositor and desktop environment with GPU acceleration
- Linux binary compatibility layer called Bridge
- Native ports of GCC, Chromium, OpenJDK, Discord

The first public release will target machines with Radeon 860M or 840M (RDNA 3.5) and RTL8852BE or RTL8822CE WiFi. Intel Iris Xe support is planned for a later release.

## Design Goals

**No legacy baggage.** KernOS does not support 32-bit mode, legacy BIOS, or the 8259 PIC. The minimum target is any modern x86-64 UEFI machine.

**Vulkan only.** No OpenGL, no legacy graphics path. The GPU driver targets Vulkan from the start. This keeps the graphics stack clean and aligns with where the industry is going.

**Lua as a first-class scripting layer.** System configuration, desktop scripting, and application extensions are written in Lua. The UI toolkit will support both C++23 and Lua officially.

**Linux compatibility via Bridge.** A userspace translation layer maps Linux syscalls to KernOS syscalls, allowing Linux x86-64 binaries to run without modification. This includes running Proton for Windows game compatibility.

## License

GPL-3.0
