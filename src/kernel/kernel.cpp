#include "framebuffer/framebuffer.hpp"
#include "TTY/TTY.hpp"
#include "GDT/GDT.hpp"
#include "IDT/IDT.hpp"

static void halt()
{
	while (true)
	{
		__asm__ ("hlt");

	}
}

extern "C" void kernelMain()
{
    GDT::init();
    IDT::init();
    __asm__("sti");

    if (!Framebuffer::checkFramebuffer())
    {
        halt();
    }
    uint32_t fg = 0xFF0000;
    uint32_t bg = 0x00000000;

    TTY::putString("Hello, this is KernOS", 2, 2, fg, bg);
    TTY::putString("https://github.com/plexescor/KernOS", 2, 3, fg, bg);
    TTY::putString("6769", 2, 4, fg, bg);

    while (true) {}
}
