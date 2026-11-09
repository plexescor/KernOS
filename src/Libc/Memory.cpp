#include "Libc/Memory.hpp"
#include <cstdint>
#include <cstddef>

void Libc::Memory::memset(void* address, uint8_t value, size_t size)
{
    uint8_t* bytePtr = (uint8_t*)address;
    for (size_t i = 0; i < size; i++)
    {
        bytePtr[i] = value;
    }
}
