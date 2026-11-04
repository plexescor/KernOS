#pragma once
#include <stdint.h>
#include <stddef.h>

void __libc_putChar(uint16_t c, size_t cX, size_t cY, uint32_t fg, uint32_t bg);
void __libc_putString(const char* str, size_t startX, size_t startY, uint32_t fg, uint32_t bg);