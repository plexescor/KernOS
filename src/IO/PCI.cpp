#include "IO/PCI.hpp"
#include "Memory/VirtualMemoryManager.hpp"
#include "TTY/TTY.hpp"

#include <cstdint>
extern void uint64ToHex(uint64_t value, char* buffer);

static uint64_t PCIeConfigBase = 0;
static uint64_t startBus = 0;
static uint64_t endBus = 0;

extern VirtualMemoryManager* vmm_G;

static uint64_t BAR0_NVME = 0;

uint64_t IO::PCI::getECAMPhysical(uint8_t bus, uint8_t device, uint8_t function)
{
    return PCIeConfigBase
        | ((uint64_t)(bus      & 0xFF)  << 20)
        | ((uint64_t)(device   & 0x1F)  << 15)
        | ((uint64_t)(function & 0x07)  << 12);
}

void IO::PCI::PCIInit(uint64_t configBase, uint64_t busStart, uint64_t busEnd)
{
    PCIeConfigBase = configBase;
    startBus = busStart;
    endBus = busEnd;
}

void IO::PCI::enumerate()
{
    for (uint64_t bus = startBus; bus <= endBus; bus++)
    {
        for (uint64_t device = 0; device < 32; device++)
        {
            for (uint64_t function = 0; function < 8; function++)
            {
                uint64_t physicalAddress = IO::PCI::getECAMPhysical(bus, device, function);
                uint64_t virtualAddress = vmm_G->physicalToVirtual(physicalAddress);
                vmm_G->mapPage(virtualAddress, physicalAddress, PAGE_CACHE_DISABLE | PAGE_NX | PAGE_WRITABLE | PAGE_PRESENT);

                PCIConfigHeader* configHeader = (PCIConfigHeader*)virtualAddress;
                if (configHeader->vendorID == 0xFFFF) continue; // slot empty

                // Found NVME
                if (configHeader->classCode == IO::PCI::Class::MassStorage
                    && configHeader->subclass == IO::PCI::Subclass::NVMController
                    && configHeader->progIF == IO::PCI::ProgIF::NVMe)
                {
                    uint64_t BAR0Low  = configHeader->BAR0 & ~0xFULL;
                    uint64_t BAR0High = configHeader->BAR1;
                    BAR0_NVME = BAR0Low | (BAR0High << 32);
                }
            }
        }
    }
}

uint64_t IO::PCI::getBAR0_NVME()
{
    return BAR0_NVME;
}
