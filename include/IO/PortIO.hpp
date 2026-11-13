#pragma once

#include <cstdint>

namespace IO
{
    namespace Port
    {
        void outb(uint16_t port, uint8_t val);
        uint8_t inb(uint16_t port);
        void IOWait(void);
    } // Port
} // IO
