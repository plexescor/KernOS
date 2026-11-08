#pragma once
#include <cstdint>

namespace CPU
{
    namespace MSR
    {
        void getMSR(uint32_t address, uint32_t* eax, uint32_t* edx);
        void writeMSR(uint32_t address, uint64_t value);
    }
}
