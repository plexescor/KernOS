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
    TTY::putString("\n", TTY::Color::WHITE);
    TTY::putString("[BOOT] ", TTY::Color::CYAN);
    TTY::putString("Starting KernOS...\n", TTY::Color::WHITE);

    if (!Framebuffer::checkFramebuffer())
        halt();

    // GDT
    TTY::putString("[INIT] ", TTY::Color::CYAN);
    TTY::putString("Initializing GDT...\n", TTY::Color::WHITE);

    GDT::init();

    TTY::putString("[ OK ] ", TTY::Color::LIME);
    TTY::putString("GDT initialized successfully.\n", TTY::Color::WHITE);

    // IDT
    TTY::putString("[INIT] ", TTY::Color::CYAN);
    TTY::putString("Initializing IDT...\n", TTY::Color::WHITE);

    IDT::init();

    TTY::putString("[ OK ] ", TTY::Color::LIME);
    TTY::putString("IDT initialized successfully.\n", TTY::Color::WHITE);

    // Physical Memory Manager
    TTY::putString("[INIT] ", TTY::Color::CYAN);
    TTY::putString("Initializing Physical Memory Manager...\n", TTY::Color::WHITE);

    PhysicalMemoryManager pmm;

    TTY::putString("[TEST] ", TTY::Color::CYAN);
    TTY::putString("Allocating physical frame...\n", TTY::Color::WHITE);

    uint64_t memory = pmm.allocFrame_PMM();

    if (!memory)
    {
        TTY::putString("[FAIL] ", TTY::Color::RED);
        TTY::putString("Physical frame allocation failed!\n", TTY::Color::WHITE);
        halt();
    }

    char buffer[21];

    uint64ToHex(memory, buffer);

    TTY::putString("[ OK ] ", TTY::Color::LIME);
    TTY::putString("Physical frame allocated at: ", TTY::Color::WHITE);
    TTY::putString(buffer, TTY::Color::PURPLE);
    TTY::putString("\n", TTY::Color::WHITE);

    pmm.freeFrame_PMM(memory);

    TTY::putString("[ OK ] ", TTY::Color::LIME);
    TTY::putString("Physical frame freed.\n", TTY::Color::WHITE);

    struct limine_memmap_response* memoryMap = pmm.getMemoryMap_Limine();

    // Virtual Memory Manager
    TTY::putString("[INIT] ", TTY::Color::CYAN);
    TTY::putString("Initializing Virtual Memory Manager...\n", TTY::Color::WHITE);

    struct limine_hhdm_response* hddmResponse = pmm.getHDDM();

    VirtualMemoryManager vmm(&pmm, memoryMap, hddmResponse);

    TTY::putString("[ OK ] ", TTY::Color::LIME);
    TTY::putString("Virtual Memory Manager initialized.\n", TTY::Color::WHITE);

    // LAPIC
    CPU::APIC::setVMM(&vmm);

    TTY::putString("[INIT] ", TTY::Color::CYAN);
    TTY::putString("Initializing Local APIC...\n", TTY::Color::WHITE);

    if (!CPU::APIC::initialiseLAPIC())
    {
        TTY::putString("[FAIL] ", TTY::Color::RED);
        TTY::putString("LAPIC initialization failed. System halted.\n", TTY::Color::WHITE);
        halt();
    }

    TTY::putString("[ OK ] ", TTY::Color::LIME);
    TTY::putString("Local APIC initialized.\n", TTY::Color::WHITE);

    // Keyboard interrupts
    TTY::putString("[INIT] ", TTY::Color::CYAN);
    TTY::putString("Setting up keyboard interrupts...\n", TTY::Color::WHITE);

    CPU::APIC::IOAPICSetupKeyboard(0x21, 0);

    TTY::putString("[ OK ] ", TTY::Color::LIME);
    TTY::putString("Keyboard interrupt configured.\n", TTY::Color::WHITE);

    __asm__("sti");

    // Heap mapping
    TTY::putString("[INIT] ", TTY::Color::CYAN);
    TTY::putString("Mapping kernel heap...\n", TTY::Color::WHITE);

    uint64_t heapPages = KERNEL_HEAP_INITIAL / 4096;

    for (uint64_t page = 0; page < heapPages; page++)
    {
        uint64_t virtualAddress = KERNEL_HEAP_START + (page * 4096);
        uint64_t physicalAddress = pmm.allocFrame_PMM();

        if (!physicalAddress)
        {
            TTY::putString("[FAIL] ", TTY::Color::RED);
            TTY::putString("Out of physical memory while mapping heap!\n", TTY::Color::WHITE);
            halt();
        }

        vmm.mapPage(virtualAddress, physicalAddress, PAGE_PRESENT | PAGE_WRITABLE);
    }

    TTY::putString("[ OK ] ", TTY::Color::LIME);
    TTY::putString("Kernel heap mapped successfully.\n", TTY::Color::WHITE);

    bool heapInitResult = Libc::Memory::kmallocInit(KERNEL_HEAP_START, KERNEL_HEAP_INITIAL, &vmm, &pmm);
    if (!heapInitResult)
    {
        TTY::putString("[FAIL] ", TTY::Color::RED);
        TTY::putString("Kernel heap initialization failed. System halted.\n", TTY::Color::WHITE);
        halt();
    }
    TTY::putString("[ OK ] ", TTY::Color::LIME);
    TTY::putString("Kernel heap initialised successfully with size: ", TTY::Color::WHITE);
    char buffer2[21];
    TTY::putString(uint64ToString((KERNEL_HEAP_INITIAL/(1024)), buffer2, 21), TTY::Color::YELLOW);
    TTY::putString(" Kilobytes\n", TTY::Color::YELLOW);

    // heap tests
    TTY::putString("[INFO] ", TTY::Color::WHITE);
    TTY::putString("Starting Heap tests...\n", TTY::Color::WHITE);

    char oomBuf[21];
    uint64_t total = 0;
    void* ptr;
    while ((ptr = Libc::Memory::kmalloc(1024 * 1024)) != nullptr)
    {
        total += 1024 * 1024;
    }
    TTY::putString("[TEST] OOM hit after: ", TTY::Color::CYAN);
    TTY::putString(uint64ToString(total / (1024 * 1024), oomBuf, 21), TTY::Color::YELLOW);
    TTY::putString(" MB\n", TTY::Color::WHITE);

    // Finished
    TTY::putString("[ OK ] ", TTY::Color::LIME);
    TTY::putString("All initialization stages completed.\n", TTY::Color::WHITE);

    TTY::putString("[BOOT] ", TTY::Color::CYAN);
    TTY::putString("Welcome to KernOS! Try typing...\n", TTY::Color::WHITE);


    while (true)
        __asm__("hlt");
}
