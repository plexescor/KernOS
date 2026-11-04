#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"

extern volatile struct limine_framebuffer_request framebufferRequest;

bool __framebuffer_checkFramebuffer();
struct limine_framebuffer* __framebuffer_getFramebuffer(); 