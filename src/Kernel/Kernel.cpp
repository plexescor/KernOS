#include "Framebuffer/Framebuffer.hpp"
#include "TTY/TTY.hpp"
#include "GDT/GDT.hpp"
#include "IDT/IDT.hpp"
#include "CPU/APIC.hpp"

#include "Memory/PhysicalMemoryManager.hpp"

static void halt()
{
	while (true)
	{
		__asm__ ("hlt");

	}
}

#include <stdint.h>

const char* uint64ToString(uint64_t value, char* bufferOut, int bufferSize) {
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
    TTY::putString("Welcome to KernOS! Try typing...\n", fg, bg);

    PhysicalMemoryManager pmm;

    uint64_t memory = pmm.allocFrame_PMM();
    pmm.freeFrame_PMM(memory);

    TTY::putString("Welcome to KernOS! Try typing...\n", fg, bg);
    while (true) {}
}
