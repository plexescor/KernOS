#include "framebuffer/framebuffer.h"

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
}