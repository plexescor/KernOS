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

    CPU::APIC::setVMM(&vmm);
    TTY::putString("Intialising LAPIC...\n", fg, bg);
    if (!CPU::APIC::initialiseLAPIC())
    {
        TTY::putString("Critical Error: Core hardware does not support LAPIC.\nThe system will halt...", fg_Error, bg);
        halt();
    }

    TTY::putString("Setting up Keyboard Interrupts...\n", fg, bg);
    // 33 in decimal
    CPU::APIC::IOAPICSetupKeyboard(0x21, 0);

    __asm__("sti");



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

    // --- new/delete operator tests ---
    TTY::putString("Testing new and delete operators...\n", fg, bg);

    Libc::Memory::kmallocInit(KERNEL_HEAP_START, KERNEL_HEAP_INITIAL);
    // Test 1: basic new
    uint64_t* ptr1 = new uint64_t(0xDEADBEEF);
    if (ptr1 && *ptr1 == 0xDEADBEEF)
        TTY::putString("PASS: new uint64_t\n", fg, bg);
    else
        TTY::putString("FAIL: new uint64_t\n", fg_Error, bg);

    // Test 2: basic delete (no crash = pass)
    delete ptr1;
    TTY::putString("PASS: delete uint64_t (no crash)\n", fg, bg);

    // Test 3: new array
    uint64_t* arr = new uint64_t[4];
    arr[0] = 1; arr[1] = 2; arr[2] = 3; arr[3] = 4;
    if (arr && arr[0] == 1 && arr[1] == 2 && arr[2] == 3 && arr[3] == 4)
        TTY::putString("PASS: new uint64_t[4]\n", fg, bg);
    else
        TTY::putString("FAIL: new uint64_t[4]\n", fg_Error, bg);

    // Test 4: delete array (no crash = pass)
    delete[] arr;
    TTY::putString("PASS: delete[] uint64_t (no crash)\n", fg, bg);

    // Test 5: reuse after delete
    uint64_t* ptr2 = new uint64_t(0xCAFEBABE);
    if (ptr2 && *ptr2 == 0xCAFEBABE)
        TTY::putString("PASS: new after delete reuses heap\n", fg, bg);
    else
        TTY::putString("FAIL: new after delete\n", fg_Error, bg);
    delete ptr2;

    TTY::putString("\nWelcome to KernOS! Try typing...\n", fg, bg);
    while (true) { __asm__("hlt"); }
}
