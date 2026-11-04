#include "framebuffer/framebuffer.h"
#include "libc/libc.h"

static void halt()
{
	while (true) 
	{
		__asm__ ("hlt");

	}
}

void kernelMain()
{
    if (!__framebuffer_checkFramebuffer())
    {
        halt();
    }
    uint32_t fg = 0xFFFFFFFF;
    uint32_t bg = 0x00000000;

    __libc_putString("Hello, Kernel World!", 2, 2, fg, bg);
    __libc_putString("PSF font rendering works!", 2, 3, fg, bg);
    __libc_putString("Line three: 0123456789", 2, 4, fg, bg);

    while (true) {}
}