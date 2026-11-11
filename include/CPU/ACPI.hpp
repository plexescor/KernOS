#pragma once
#include "Memory/VirtualMemoryManager.hpp"
#include "Memory/PhysicalMemoryManager.hpp"

namespace CPU
{
    namespace ACPI
    {
        struct __attribute__((packed)) ACPIDescriptionHeader
        {
            char signature[4];          // e.g., "RSDT", "FACP", "APIC"
            uint32_t length;            // Total length of the table, including header
            uint8_t revision;
            uint8_t checksum;           // Entire table must sum to 0
            char OEMID[6];
            char OEMTableID[8];
            uint32_t OEMRevision;
            uint32_t creatorID;
            uint32_t creatorRevision;
        };

        struct __attribute__((packed)) MADTTable
        {
            ACPIDescriptionHeader header;
            uint32_t localAPICAddress;
            uint32_t flags;
            // records start here
        };

        struct __attribute__((packed)) MADTRecordHeader
        {
            uint8_t type; // 1 = MADTIOAPIC, 2 = MADTInterruptSourceOverride
            uint8_t length;
        };

        struct __attribute__((packed)) MADTInterruptSourceOverride
        {
            MADTRecordHeader header;
            uint8_t  bus;
            uint8_t  irqSource;
            uint32_t gsi;
            uint16_t flags;
        };

        struct __attribute__((packed)) MADTIOAPIC
        {
            MADTRecordHeader header;
            uint8_t  IOAPICID;
            uint8_t  reserved;
            uint32_t IOAPICAddress;
            uint32_t gsiBase;
        };

        struct __attribute__((packed)) XSDTTable
        {
            ACPIDescriptionHeader header;
            uint64_t entries[];   // flexible array — 64-bit physical addresses
        };

        enum class AddressSpace : uint8_t
        {
            SYSTEM_MEMORY = 0,
            SYSTEM_IO = 1,
            PCI_CONFIG = 2,
            EMEBEDDED_CONTROLLER = 3,
            SM_BUS = 4,
            SYSTEM_CMOS = 5,
            PCI_BAR_TARGET = 6,
            IPMI = 7,
            GENERAL_PURPOSE_IO = 8,
            GENERAL_SERIAL_BUS = 9,
            PLATFORM_COMMUNICATIONS_CHANNEL = 10,
            FUNCTIONAL_FIXED_HARDWARE = 0x7F
        };

        enum class AccessSize : uint8_t
        {
            UNDEFINED = 0,
            BYTE = 1,
            WORD = 2,
            DWORD = 3,
            QWORD = 4
        };

        struct __attribute__((packed)) GenericAddressStructure
        {
            AddressSpace addressSpaceId;
            uint8_t registerBitWidth;
            uint8_t registerBitOffset;
            AccessSize accessSize;
            uint64_t address;
        };

        struct __attribute__((packed)) RootSystemDescriptionPointer
        {
            // ACPI 1.0 Fields (20 bytes)
            char signature[8];          // "RSD PTR "
            uint8_t checksum;           // Checksum of first 20 bytes
            char OEMID[6];
            uint8_t revision;           // 0 = ACPI 1.0, 2 = ACPI 2.0+
            uint32_t RSDTAddress;       // 32-bit physical address of RSDT

            // ACPI 2.0+ Extended Fields (Total 36 bytes)
            uint32_t length;            // Total length of RSDP (36)
            uint64_t XSDTAddress;       // 64-bit physical address of XSDT
            uint8_t extendedChecksum;   // Checksum of entire 36-byte structure
            uint8_t reserved[3];
        };

        struct __attribute__((packed)) MCFGTable
        {
            ACPIDescriptionHeader header;
            uint64_t reserved;
            // followed by n entries
        };

        struct __attribute__((packed)) MCFGEntry
        {
            uint64_t baseAddress;   // physical base of PCIe config space
            uint16_t segmentGroup;
            uint8_t  startBus;
            uint8_t  endBus;
            uint32_t reserved;
        };

        ACPIDescriptionHeader* findTable(const char signature[4]);
        bool iterateMADTTable();
        bool iterateMCFGTable();
        bool ACPI_Initialise(VirtualMemoryManager* vmm_P, PhysicalMemoryManager* pmm_P);
    } // ACPI
} // CPU
