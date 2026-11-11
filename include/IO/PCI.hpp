#pragma once
#include "Memory/VirtualMemoryManager.hpp"
#include <cstdint>

namespace IO::PCI
{
    struct __attribute__((packed)) PCIConfigHeader
    {
        uint16_t vendorID;        // 0x00
        uint16_t deviceID;        // 0x02
        uint16_t command;         // 0x04
        uint16_t status;          // 0x06
        uint8_t  revisionID;      // 0x08
        uint8_t  progIF;          // 0x09
        uint8_t  subclass;        // 0x0A
        uint8_t  classCode;       // 0x0B
        uint8_t  cacheLineSize;   // 0x0C
        uint8_t  latencyTimer;    // 0x0D
        uint8_t  headerType;      // 0x0E
        uint8_t  BIST;            // 0x0F
        uint32_t BAR0;            // 0x10
        uint32_t BAR1;            // 0x14
        uint32_t BAR2;            // 0x18
        uint32_t BAR3;            // 0x1C
        uint32_t BAR4;            // 0x20
        uint32_t BAR5;            // 0x24
    };

    namespace Class
    {
        constexpr uint8_t MassStorage = 0x01;
    }
    namespace Subclass
    {
        constexpr uint8_t NVMController = 0x08;
    }
    namespace ProgIF
    {
        constexpr uint8_t NVMe = 0x02;
    }

    void PCIInit(uint64_t configBase, uint64_t busStart, uint64_t busEnd, VirtualMemoryManager* vmm_P);
    uint64_t getECAMPhysical(uint8_t bus, uint8_t device, uint8_t function);
    void enumerate();
    uint64_t getBAR0_NVME();
}
