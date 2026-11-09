#include "Framebuffer/Framebuffer.hpp"
#include "TTY/TTY.hpp"
#include "GDT/GDT.hpp"
#include "IDT/IDT.hpp"
#include "CPU/APIC.hpp"

static void halt()
{
	while (true)
	{
		__asm__ ("hlt");

	}
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

    TTY::putString("Welcome to KernOS! Try typing...\n", fg, bg);

    while (true) {}
}
