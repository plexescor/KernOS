#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"

extern volatile struct limine_framebuffer_request framebufferRequest;

bool __framebuffer_checkFramebuffer();
struct limine_framebuffer* __framebuffer_getFramebuffer(uint64_t index);
uint64_t __framebuffer_getFramebufferPitch(struct limine_framebuffer* fb);
uint64_t __framebuffer_getFramebufferCount(); 