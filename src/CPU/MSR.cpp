#include "CPU/MSR.hpp"
#include <cstdint>

void CPU::MSR::getMSR(uint32_t address, uint32_t* eax, uint32_t* edx)
{
    __asm__ (
        "rdmsr"
        : "=a"(*eax), "=d"(*edx)
        : "c"(address)
    );
}

void CPU::MSR::writeMSR(uint32_t address, uint64_t value)
{
    uint32_t low = (uint32_t)(value & 0xFFFFFFFF);
    uint32_t high = (uint32_t)(value >> 32);
    __asm__ (
        "wrmsr"
        :
        : "c"(address), "a"(low), "d"(high)
        : "memory"
    );
}
