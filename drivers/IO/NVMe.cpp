#include "Drivers/IO/NVMe.hpp"
#include "IO/PCI.hpp"
#include "TTY/TTY.hpp"
#include <cstdint>

uint64_t NVMEBAR0 = 0;

extern void uint64ToHex(uint64_t value, char* buffer);

bool Driver::IO::NVMe::initialise()
{
    NVMEBAR0 = ::IO::PCI::getBAR0_NVME();
    char buffer[21];
    uint64ToHex(NVMEBAR0, buffer);
    ::TTY::putString("[TEST] ", ::TTY::Color::CYAN);
    ::TTY::putString("NVME BAR0 Address Hex: ", ::TTY::Color::WHITE);
    ::TTY::putString(buffer, ::TTY::Color::PURPLE);
    ::TTY::putString("\n", TTY::Color::WHITE);
    return true;
}
