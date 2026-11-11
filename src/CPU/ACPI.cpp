#include "CPU/ACPI.hpp"
#include "CPU/APIC.hpp"
#include "IO/PortIO.hpp"
#include "Memory/VirtualMemoryManager.hpp"
#include "Memory/PhysicalMemoryManager.hpp"
#include "TTY/TTY.hpp"

#include <cstdint>

static VirtualMemoryManager* vmm = nullptr;
static PhysicalMemoryManager* pmm = nullptr;


#define LIMINE_RSDP_REQUEST { LIMINE_COMMON_MAGIC, 0xc5e77b6b397e7b43, 0x27637845accdcf3c }

__attribute__((used, section(".limine_requests")))
volatile struct limine_rsdp_request RSDPRequest =
{
    .id = LIMINE_RSDP_REQUEST,
    .revision = 0,
    .response = NULL
};

// This would be a virtual address
static void* RSDPAddress = nullptr;
static struct CPU::ACPI::XSDTTable* XSDT = nullptr;
static struct CPU::ACPI::MADTTable* MADTTable_ = nullptr;

CPU::ACPI::ACPIDescriptionHeader* CPU::ACPI::findTable(const char signature[4])
{
    uint64_t numberOfEntries = (XSDT->header.length - sizeof(CPU::ACPI::ACPIDescriptionHeader)) / 8;

    for (uint64_t entry = 0; entry < numberOfEntries; entry++)
    {
        CPU::ACPI::ACPIDescriptionHeader* header = (CPU::ACPI::ACPIDescriptionHeader*)(vmm->physicalToVirtual(XSDT->entries[entry]));

        if (header->signature[0] == signature[0]
            && header->signature[1] == signature[1]
            && header->signature[2] == signature[2]
            && header->signature[3] == signature[3])
        {
            return header;
        }
    }
    return nullptr;
}
bool CPU::ACPI::iterateMADTTable()
{
    MADTTable_ = (CPU::ACPI::MADTTable*)(CPU::ACPI::findTable("APIC"));
    if (!MADTTable_)
    {
        TTY::putString("[FAIL] ", TTY::Color::RED);
        TTY::putString("Couldn't find \"MADTTable\" Entry in XSDT!\n", TTY::Color::WHITE);
        return false;
    }

    uint8_t* ptr = (uint8_t*)MADTTable_ + sizeof(CPU::ACPI::MADTTable);
    uint8_t* end = (uint8_t*)MADTTable_ + MADTTable_->header.length;

    uint64_t IOAPICAddress = 0;
    uint32_t keyboardGSI = 1;
    uint16_t keyboardFlags = 0;
    uint64_t LAPICAddress = MADTTable_->localAPICAddress;
    while (ptr < end)
    {
        CPU::ACPI::MADTRecordHeader* record = (CPU::ACPI::MADTRecordHeader*)ptr;
        if (record->type == 1)
        {
            CPU::ACPI::MADTIOAPIC* MADTIOAPIC_ = (CPU::ACPI::MADTIOAPIC*)ptr;
            IOAPICAddress = MADTIOAPIC_->IOAPICAddress;
        }
        if (record->type == 2)
        {
            CPU::ACPI::MADTInterruptSourceOverride* MADTISO = (CPU::ACPI::MADTInterruptSourceOverride*)ptr;
            // IRQ 1 == keyboard
            if (MADTISO->irqSource == 1)
            {
                keyboardGSI = MADTISO->gsi;
                keyboardFlags = MADTISO->flags;
            }
        }
        ptr += record->length;
    }

    // TTY::putString("IOAPICAddress found: ", TTY::Color::WHITE);
    // printHex(IOAPICAddress);
    // Only update these values when we find them properly
    CPU::APIC::initialiseLAPIC(LAPICAddress);
    if (IOAPICAddress != 0)
        CPU::APIC::initialiseIOAPIC(IOAPICAddress, keyboardGSI, keyboardFlags);
    return true;
}

bool CPU::ACPI::ACPI_Initialise(VirtualMemoryManager *vmm_P, PhysicalMemoryManager *pmm_P)
{
    if (!vmm_P || !pmm_P)
    {
        TTY::putString("[FAIL] ", TTY::Color::RED);
        TTY::putString("Memory Manager pointers are Null!\n", TTY::Color::WHITE);
        return false;
    }
    vmm = vmm_P;
    pmm = pmm_P;

    if (RSDPRequest.response != NULL)
    {
        // Limine already gives us virtual address
        RSDPAddress = RSDPRequest.response->address;
    }

    if (!RSDPAddress)
    {
        TTY::putString("[FAIL] ", TTY::Color::RED);
        TTY::putString("RSDP address is null!\n", TTY::Color::WHITE);
    }
    // Check revision == 2 to find if its acio 2.0, 15 offset
    struct CPU::ACPI::RootSystemDescriptionPointer* RSDP = (struct CPU::ACPI::RootSystemDescriptionPointer*)RSDPAddress;
    if (RSDP->revision < 2)
    {
        TTY::putString("[FAIL] ", TTY::Color::RED);
        TTY::putString("KernOS only supports ACPI revision 2.0!\n", TTY::Color::WHITE);
        return false;
    }

    // Now get XSDT Address, 24 offset
    XSDT = (CPU::ACPI::XSDTTable*)(vmm->physicalToVirtual(RSDP->XSDTAddress));
    if (!XSDT)
    {
        TTY::putString("[FAIL] ", TTY::Color::RED);
        TTY::putString("Couldn't get XSDT address due to unknown error!\n", TTY::Color::WHITE);
        return false;
    }
    if (XSDT->header.signature[0] != 'X' || XSDT->header.signature[1] != 'S'
        || XSDT->header.signature[2] != 'D' || XSDT->header.signature[3] != 'T')
    {
        TTY::putString("[FAIL] ", TTY::Color::RED);
        TTY::putString("Invalid XSDT signature!\n", TTY::Color::WHITE);
        return false;
    }

    // Our wanted entries start at offset 44
    if (!CPU::ACPI::iterateMADTTable()) return false;
    return true;
}
