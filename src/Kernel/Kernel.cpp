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
    uint32_t fg = 0xFF0000;
    uint32_t bg = 0x00000000;

    GDT::init();
    IDT::init();
    __asm__("sti");

    if (!Framebuffer::checkFramebuffer())
    {
        halt();
    }
    if (!CPU::APIC::initialiseAPIC())
    {
        TTY::putString("Critical Error: Core hardware does not support APIC.\nThe system will halt...", 1, 1, fg, bg);
        halt();
    }

    TTY::putString("Hello, this is KernOS", 1, 2, fg, bg);
    TTY::putString("https://github.com/plexescor/KernOS", 1, 3, fg, bg);
    TTY::putString("6769", 1, 4, fg, bg);

    while (true) {}
}
