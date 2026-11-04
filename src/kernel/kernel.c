#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"

__attribute__((used, section(".limine_requests_start")))
volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
volatile uint64_t limineBaseRevision[3] = { 0xf9562b2d5c95a6c8, 0x6a7b384944536bdc, 0 };

__attribute__((used, section(".limine_requests")))
volatile struct limine_framebuffer_request framebufferRequest = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
    .response = NULL
};

__attribute__((used, section(".limine_requests_end")))
volatile LIMINE_REQUESTS_END_MARKER;


static void halt()
{
	while (true) 
	{
		__asm__ ("hlt");

	}
}


// Scale an 8-bit colour channel value to the size the framebuffer gives the
// channel and move it into place within a pixel.
static uint32_t fb_channel(uint8_t value, uint8_t mask_size, uint8_t mask_shift)
{
    uint64_t max = ((uint64_t)1 << mask_size) - 1;
    return (uint32_t)((value * max / 255) << mask_shift);
}

// Build a pixel from 8-bit red, green and blue values following the channel
// layout of the framebuffer.
static uint32_t fb_pixel(struct limine_framebuffer *fb, uint8_t red, uint8_t green, uint8_t blue)
{
    return fb_channel(red, fb->red_mask_size, fb->red_mask_shift)
         | fb_channel(green, fb->green_mask_size, fb->green_mask_shift)
         | fb_channel(blue, fb->blue_mask_size, fb->blue_mask_shift);
}

// Print a nice pattern to a framebuffer as an example.
static void fb_pattern(struct limine_framebuffer *fb)
{
    volatile uint32_t* fb_ptr = (volatile uint32_t*)fb->address;
    for (size_t y = 0; y < fb->height; y++)
	{
        for (size_t x = 0; x < fb->width; x++)
		{
			uint64_t byte_offset = (y * fb->pitch) + (x * (fb->bpp / 8));
			volatile uint32_t* pixel = (volatile uint32_t*)((uintptr_t)fb_ptr + byte_offset);
            uint8_t nX = x * 255 / fb->width;
            uint8_t nY = y * 255 / fb->height;
            *pixel = fb_pixel(fb, 0, nY, nX);
        }
    }
}

void kernelMain()
{

    // Ensure we got a framebuffer.
    if (framebufferRequest.response == NULL
     	|| framebufferRequest.response->framebuffer_count < 1) 
	{
        halt();
    }

    // Print the pattern to every framebuffer.
    for (uint64_t i = 0; i < framebufferRequest.response->framebuffer_count; i++) 
	{
        struct limine_framebuffer* framebuffer = framebufferRequest.response->framebuffers[i];

        // Ensure the framebuffer has 32-bit RGB pixels, the only kind we handle.
        if (framebuffer->memory_model != LIMINE_FRAMEBUFFER_RGB || framebuffer->bpp != 32) 
		{
            halt();
        }

		fb_pattern(framebuffer);
    }
	while (true) {}
}