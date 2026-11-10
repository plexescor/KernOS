#pragma once
#include <cstdint>
#include <cstddef>

namespace TTY
{
    enum class Color : uint32_t
    {
        BLACK       = 0x000000,
        WHITE       = 0xFFFFFF,
        RED         = 0xFF0000,
        GREEN       = 0x00FF00,
        BLUE        = 0x0000FF,
        YELLOW      = 0xFFFF00,
        CYAN        = 0x00FFFF,
        MAGENTA     = 0xFF00FF,

        GRAY        = 0x808080,
        DARK_GRAY   = 0x404040,
        LIGHT_GRAY  = 0xC0C0C0,

        ORANGE      = 0xFFA500,
        PURPLE      = 0x800080,
        PINK        = 0xFFC0CB,
        BROWN       = 0xA52A2A,

        NAVY        = 0x000080,
        TEAL        = 0x008080,
        LIME        = 0x00FF00,
        OLIVE       = 0x808000,
        MAROON      = 0x800000,
        SILVER      = 0xC0C0C0
    };
    void putChar(uint16_t c, size_t cX, size_t cY, Color foregroundColor, Color backgroundColor);
    void putString(const char* str, Color foregroundColor, Color backgroundColor);
} // TTY
