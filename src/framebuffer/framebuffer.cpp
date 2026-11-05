#include <cstdint>
#include <cstddef>

#include "limine.h"
#include "framebuffer/framebuffer.hpp"

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


bool Framebuffer::checkFramebuffer()
{
    // Ensure we got a framebuffer.
    if (framebufferRequest.response == NULL
     	|| framebufferRequest.response->framebuffer_count < 1) 
	{
        return false;
    }

    // Print the pattern to every framebuffer.
    for (uint64_t i = 0; i < framebufferRequest.response->framebuffer_count; i++) 
	{
        struct limine_framebuffer* framebuffer = framebufferRequest.response->framebuffers[i];

        // Ensure the framebuffer has 32-bit RGB pixels, the only kind we handle.
        if (framebuffer->memory_model != LIMINE_FRAMEBUFFER_RGB || framebuffer->bpp != 32) 
		{
            return false;
        }
    }
    
    return true;
}

struct limine_framebuffer* Framebuffer::getFramebuffer(uint64_t index)
{
    return framebufferRequest.response->framebuffers[index];
}

uint64_t Framebuffer::getFramebufferPitch(struct limine_framebuffer* fb)
{
    return fb->pitch;
}

uint64_t Framebuffer::getFramebufferCount()
{
    return framebufferRequest.response->framebuffer_count;
}