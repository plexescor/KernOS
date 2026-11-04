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
    uint32_t fg = 0xFF0000;
    uint32_t bg = 0x00000000;

    __libc_putString("Hello, this is TantrumsOS", 2, 2, fg, bg);
    __libc_putString("https://github.com/plexescor/TantrumsOS", 2, 3, fg, bg);
    __libc_putString("6769", 2, 4, fg, bg);

    while (true) {}
}