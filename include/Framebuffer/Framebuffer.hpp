#pragma once

#include <cstdint>
#include <cstddef>
#include "limine.h"

extern volatile struct limine_framebuffer_request framebufferRequest;

namespace Framebuffer
{
    bool checkFramebuffer();
    struct limine_framebuffer* getFramebuffer(uint64_t index);
    uint64_t getFramebufferPitch(struct limine_framebuffer* fb);
    uint64_t getFramebufferCount(); 
} // Framebuffer