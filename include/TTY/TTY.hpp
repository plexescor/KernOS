#pragma once
#include <cstdint>
#include <cstddef>

namespace TTY
{
    void putChar(uint16_t c, size_t cX, size_t cY, uint32_t fg, uint32_t bg);
    void putString(const char* str, uint32_t fg, uint32_t bg);
} // TTY
