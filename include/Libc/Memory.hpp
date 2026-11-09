#pragma once
#include <cstdint>
#include <cstddef>

namespace Libc
{
    namespace Memory
    {
        void memset(void* address, uint8_t value, size_t size);
    }
}
