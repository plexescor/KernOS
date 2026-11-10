#include "Framebuffer/Framebuffer.hpp"
#include "Libc/Memory.hpp"
#include "TTY/TTY.hpp"
#include "GDT/GDT.hpp"
#include "IDT/IDT.hpp"
#include "CPU/APIC.hpp"
#include "Memory/PhysicalMemoryManager.hpp"
#include "Memory/VirtualMemoryManager.hpp"
#include "IO/PortIO.hpp"
#include "limine.h"
#define KERNEL_HEAP_START     0xFFFFB00000000000
#define KERNEL_HEAP_INITIAL   (1024ULL * 1024ULL * 16ULL)
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

    // Boot
    TTY::putString("\n", TTY::Color::WHITE, TTY::Color::BLACK);
    TTY::putString("[BOOT] ", TTY::Color::CYAN, TTY::Color::BLACK);
    TTY::putString("Starting KernOS...\n", TTY::Color::WHITE, TTY::Color::BLACK);

    if (!Framebuffer::checkFramebuffer())
        halt();

    // GDT
    TTY::putString("[INIT] ", TTY::Color::CYAN, TTY::Color::BLACK);
    TTY::putString("Initializing GDT...\n", TTY::Color::WHITE, TTY::Color::BLACK);

    GDT::init();

    TTY::putString("[ OK ] ", TTY::Color::LIME, TTY::Color::BLACK);
    TTY::putString("GDT initialized successfully.\n", TTY::Color::WHITE, TTY::Color::BLACK);

    // IDT
    TTY::putString("[INIT] ", TTY::Color::CYAN, TTY::Color::BLACK);
    TTY::putString("Initializing IDT...\n", TTY::Color::WHITE, TTY::Color::BLACK);

    IDT::init();

    TTY::putString("[ OK ] ", TTY::Color::LIME, TTY::Color::BLACK);
    TTY::putString("IDT initialized successfully.\n", TTY::Color::WHITE, TTY::Color::BLACK);

    // Physical Memory Manager
    TTY::putString("[INIT] ", TTY::Color::CYAN, TTY::Color::BLACK);
    TTY::putString("Initializing Physical Memory Manager...\n", TTY::Color::WHITE, TTY::Color::BLACK);

    PhysicalMemoryManager pmm;

    TTY::putString("[TEST] ", TTY::Color::CYAN, TTY::Color::BLACK);
    TTY::putString("Allocating physical frame...\n", TTY::Color::WHITE, TTY::Color::BLACK);

    uint64_t memory = pmm.allocFrame_PMM();

    if (!memory)
    {
        TTY::putString("[FAIL] ", TTY::Color::RED, TTY::Color::BLACK);
        TTY::putString("Physical frame allocation failed!\n", TTY::Color::WHITE, TTY::Color::BLACK);
        halt();
    }

    char buffer[21];

    uint64ToHex(memory, buffer);

    TTY::putString("[ OK ] ", TTY::Color::LIME, TTY::Color::BLACK);
    TTY::putString("Physical frame allocated at: ", TTY::Color::WHITE, TTY::Color::BLACK);
    TTY::putString(buffer, TTY::Color::PURPLE, TTY::Color::BLACK);
    TTY::putString("\n", TTY::Color::WHITE, TTY::Color::BLACK);

    pmm.freeFrame_PMM(memory);

    TTY::putString("[ OK ] ", TTY::Color::LIME, TTY::Color::BLACK);
    TTY::putString("Physical frame freed.\n", TTY::Color::WHITE, TTY::Color::BLACK);

    struct limine_memmap_response* memoryMap = pmm.getMemoryMap_Limine();

    // Virtual Memory Manager
    TTY::putString("[INIT] ", TTY::Color::CYAN, TTY::Color::BLACK);
    TTY::putString("Initializing Virtual Memory Manager...\n", TTY::Color::WHITE, TTY::Color::BLACK);

    struct limine_hhdm_response* hddmResponse = pmm.getHDDM();

    VirtualMemoryManager vmm(&pmm, memoryMap, hddmResponse);

    TTY::putString("[ OK ] ", TTY::Color::LIME, TTY::Color::BLACK);
    TTY::putString("Virtual Memory Manager initialized.\n", TTY::Color::WHITE, TTY::Color::BLACK);

    // LAPIC
    CPU::APIC::setVMM(&vmm);

    TTY::putString("[INIT] ", TTY::Color::CYAN, TTY::Color::BLACK);
    TTY::putString("Initializing Local APIC...\n", TTY::Color::WHITE, TTY::Color::BLACK);

    if (!CPU::APIC::initialiseLAPIC())
    {
        TTY::putString("[FAIL] ", TTY::Color::RED, TTY::Color::BLACK);
        TTY::putString("LAPIC initialization failed. System halted.\n", TTY::Color::WHITE, TTY::Color::BLACK);
        halt();
    }

    TTY::putString("[ OK ] ", TTY::Color::LIME, TTY::Color::BLACK);
    TTY::putString("Local APIC initialized.\n", TTY::Color::WHITE, TTY::Color::BLACK);

    // Keyboard interrupts
    TTY::putString("[INIT] ", TTY::Color::CYAN, TTY::Color::BLACK);
    TTY::putString("Setting up keyboard interrupts...\n", TTY::Color::WHITE, TTY::Color::BLACK);

    CPU::APIC::IOAPICSetupKeyboard(0x21, 0);

    TTY::putString("[ OK ] ", TTY::Color::LIME, TTY::Color::BLACK);
    TTY::putString("Keyboard interrupt configured.\n", TTY::Color::WHITE, TTY::Color::BLACK);

    __asm__("sti");

    // Heap mapping
    TTY::putString("[INIT] ", TTY::Color::CYAN, TTY::Color::BLACK);
    TTY::putString("Mapping kernel heap...\n", TTY::Color::WHITE, TTY::Color::BLACK);

    uint64_t heapPages = KERNEL_HEAP_INITIAL / 4096;

    for (uint64_t page = 0; page < heapPages; page++)
    {
        uint64_t virtualAddress = KERNEL_HEAP_START + (page * 4096);
        uint64_t physicalAddress = pmm.allocFrame_PMM();

        if (!physicalAddress)
        {
            TTY::putString("[FAIL] ", TTY::Color::RED, TTY::Color::BLACK);
            TTY::putString("Out of physical memory while mapping heap!\n", TTY::Color::WHITE, TTY::Color::BLACK);
            halt();
        }

        vmm.mapPage(virtualAddress, physicalAddress, PAGE_PRESENT | PAGE_WRITABLE);
    }

    TTY::putString("[ OK ] ", TTY::Color::LIME, TTY::Color::BLACK);
    TTY::putString("Kernel heap mapped successfully.\n", TTY::Color::WHITE, TTY::Color::BLACK);

    Libc::Memory::kmallocInit(KERNEL_HEAP_START, KERNEL_HEAP_INITIAL);
    TTY::putString("[ OK ] ", TTY::Color::LIME, TTY::Color::BLACK);
    TTY::putString("Kernel heap initialised successfully with size: ", TTY::Color::WHITE, TTY::Color::BLACK);
    char buffer2[21];
    TTY::putString(uint64ToString((KERNEL_HEAP_INITIAL/(1024)), buffer2, 21), TTY::Color::YELLOW, TTY::Color::BLACK);
    TTY::putString(" Kilobytes\n", TTY::Color::YELLOW, TTY::Color::BLACK);
    // Finished
    TTY::putString("[ OK ] ", TTY::Color::LIME, TTY::Color::BLACK);
    TTY::putString("All initialization stages completed.\n", TTY::Color::WHITE, TTY::Color::BLACK);

    TTY::putString("[BOOT] ", TTY::Color::CYAN, TTY::Color::BLACK);
    TTY::putString("Welcome to KernOS! Try typing...\n", TTY::Color::WHITE, TTY::Color::BLACK);

    while (true)
        __asm__("hlt");
}
