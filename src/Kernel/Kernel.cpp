#include "Framebuffer/Framebuffer.hpp"
#include "Libc/Memory.hpp"
#include "TTY/TTY.hpp"
#include "GDT/GDT.hpp"
#include "IDT/IDT.hpp"
#include "CPU/APIC.hpp"
#include "Memory/PhysicalMemoryManager.hpp"
#include "Memory/VirtualMemoryManager.hpp"

#include "limine.h"
#define KERNEL_HEAP_START     0xFFFFB00000000000
#define KERNEL_HEAP_INITIAL   (1024 * 1024 * 16)
#define KERNEL_HEAP_MAX       0xFFFFC00000000000

static void halt()
{
	while (true)
	{
		__asm__ ("hlt");

	}
}

#include <stdint.h>
// TODO; Change all acronyms to uppercase
const char* uint64ToString(uint64_t value, char* bufferOut, int bufferSize)
{
    if (bufferSize < 21) {
        return "";
    }

    int index = bufferSize - 1;
    bufferOut[index] = '\0';

    if (value == 0) {
        bufferOut[--index] = '0';
        return &bufferOut[index];
    }

    while (value > 0) {
        index--;
        bufferOut[index] = '0' + (value % 10);
        value /= 10;
    }

    return &bufferOut[index];
}

void uint64ToHex(uint64_t value, char* buffer)
{
    const char* digits = "0123456789ABCDEF";

    buffer[0] = '0';
    buffer[1] = 'x';

    for (int i = 0; i < 16; i++)
    {
        buffer[2 + i] = digits[(value >> (60 - i * 4)) & 0xF];
    }

    buffer[18] = '\0';
}

extern "C" void kernelMain()
{
    __asm__("cli");
    if (!Framebuffer::checkFramebuffer())
    {
        halt();
    }

    uint32_t fg = 0xFFFFFF;
    uint32_t fg_Error = 0xFF0000;
    uint32_t bg = 0x00000000;

    TTY::putString("Initialising GDT...\n", fg, bg);
    GDT::init();

    TTY::putString("Initialising IDT...\n", fg, bg);
    IDT::init();

    TTY::putString("Enabling Interrupts...\n", fg, bg);
    __asm__("sti");

    TTY::putString("Intialising LAPIC...\n", fg, bg);
    if (!CPU::APIC::initialiseLAPIC())
    {
        TTY::putString("Critical Error: Core hardware does not support LAPIC.\nThe system will halt...", fg_Error, bg);
        halt();
    }

    TTY::putString("Setting up Keyboard Interrupts...\n", fg, bg);
    // 33 in decimal
    CPU::APIC::IOAPICSetupKeyboard(0x21, 0);

    TTY::putString("Bringing PhysicalMemoryManager to life...\n", fg, bg);
    // TTY::putString("Welcome to KernOS! Try typing...\n", fg, bg);

    PhysicalMemoryManager pmm;

    uint64_t memory = pmm.allocFrame_PMM();
    char buffer[21];
    uint64ToHex(memory, buffer);
    TTY::putString(buffer, fg, bg);
    pmm.freeFrame_PMM(memory);

    struct limine_memmap_response* memoryMap = pmm.getMemoryMap_Limine();
    TTY::putString("\nBringing VirtualMemoryManager to life...\n", fg, bg);
    struct limine_hhdm_response* hddmResponse = pmm.getHDDM();
    VirtualMemoryManager vmm(&pmm, memoryMap, hddmResponse);

    TTY::putString("Mapping heap memory...\n", fg, bg);

    uint64_t heapPages = KERNEL_HEAP_INITIAL / 4096;
    for (uint64_t page = 0; page < heapPages; page++)
    {
        uint64_t virtualAddress = KERNEL_HEAP_START + (page * 4096);
        uint64_t physicalAddress = pmm.allocFrame_PMM();
        if (!physicalAddress)
        {
            TTY::putString("Critical Error: Out of Memory!!\n", fg_Error, bg);
            halt();
        }
        vmm.mapPage(virtualAddress, physicalAddress, PAGE_PRESENT | PAGE_WRITABLE);
    }

    TTY::putString("Checking heap (kmallocInit and kmalloc)...\n", fg, bg);
    // Check if pointer via kmalloc is valid or not
    Libc::Memory::kmallocInit(KERNEL_HEAP_START, KERNEL_HEAP_INITIAL);
    uint64_t f = (uint64_t)Libc::Memory::kmalloc(4096);
    uint64_t heap = (uint64_t)Libc::Memory::kmalloc(1024);
    if (!heap)
    {
        TTY::putString("Critical Error: kmalloc returned null!\n", fg_Error, bg);
        halt();
    }
    char buffer2[21];
    uint64ToHex(heap, buffer2);
    TTY::putString(buffer2, fg, bg);

    TTY::putString("\nWelcome to KernOS! Try typing...\n", fg, bg);
    while (true) {}
}
