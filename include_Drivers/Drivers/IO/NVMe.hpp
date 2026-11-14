#pragma once
#include <cstdint>

namespace Driver::IO::NVMe
{
    struct __attribute__((packed)) SubmissionQueueEntry
    {
        uint8_t opcode;
        uint8_t flags;
        uint16_t commandID;
        uint32_t NSID;
        uint64_t reserved;
        uint64_t MPTR;
        uint64_t PRP1;
        uint64_t PRP2;
        uint32_t CDW10;
        uint32_t CDW11, CDW12, CDW13, CDW14, CDW15;
    };

    #include <cstdint>

    struct __attribute__((packed)) IdentifyControllerData
    {
        // Bytes 0 - 255: Controller Capabilities and Features
        uint16_t VID;               // Bytes 0..1: PCI Vendor ID
        uint16_t SSVID;             // Bytes 2..3: PCI Subsystem Vendor ID
        char     serialNumber[20];  // Bytes 4..23: Serial Number (ASCII, space-padded)
        char     modelNumber[40];   // Bytes 24..63: Model Number (ASCII, space-padded)
        char     firmwareRev[8];    // Bytes 64..71: Firmware Revision (ASCII, space-padded)
        uint8_t  RAB;               // Byte 72: Recommended Arbitration Burst
        uint8_t  IEEE[3];           // Bytes 73..75: IEEE OUI Identifier
        uint8_t  CMIC;              // Byte 76: Controller Multi-Path I/O and Namespace Sharing
        uint8_t  MDTS;              // Byte 77: Maximum Data Transfer Size (in units of min memory page size, 2^MDTS)
        uint16_t controllerID;      // Bytes 78..79: Controller ID
        uint32_t version;           // Bytes 80..83: NVMe Specification Version
        uint32_t RTD3R;             // Bytes 84..87: RTD3 Resume Latency
        uint32_t RTD3E;             // Bytes 88..91: RTD3 Entry Latency
        uint32_t OAES;              // Bytes 92..95: Optional Asynchronous Events Supported
        uint32_t CTRATT;            // Bytes 96..99: Controller Attributes
        uint8_t  reserved1[156];    // Bytes 100..255: Reserved / Management fields

        // Bytes 256 - 511: Admin Command Set Attributes
        uint16_t OACS;              // Bytes 256..257: Optional Admin Command Support
        uint8_t  ACL;               // Byte 258: Abort Command Limit
        uint8_t  AERL;              // Byte 259: Asynchronous Event Request Limit
        uint8_t  FRMW;              // Byte 260: Firmware Updates
        uint8_t  LPA;               // Byte 261: Log Page Attributes
        uint8_t  ELPE;              // Byte 262: Error Log Page Entries
        uint8_t  NPSS;              // Byte 263: Number of Power States Support
        uint8_t  AVSCC;             // Byte 264: Admin Vendor Specific Command Configuration
        uint8_t  APSTA;             // Byte 265: Autonomous Power State Transitions
        uint16_t WCTEMP;            // Bytes 266..267: Warning Composite Temperature Threshold
        uint16_t CCTEMP;            // Bytes 268..269: Critical Composite Temperature Threshold
        uint8_t  reserved2[242];    // Bytes 270..511: Remaining Admin attributes

        // Bytes 512 - 703: NVM Command Set Attributes
        uint8_t  SQES;              // Byte 512: Submission Queue Entry Size (bits 3:0 = min, bits 7:4 = max, as 2^n)
        uint8_t  CQES;              // Byte 513: Completion Queue Entry Size (bits 3:0 = min, bits 7:4 = max, as 2^n)
        uint16_t maxCMD;            // Bytes 514..515: Maximum Outstanding Commands
        uint32_t numNamespaces;     // Bytes 516..519: Number of Namespaces (e.g., 1 or more)
        uint16_t ONCS;              // Bytes 520..521: Optional NVM Command Support
        uint8_t  reserved3[3574];   // Bytes 522..4095: Power State Descriptors & Vendor Specific
    };

    // NVMe Identify Namespace Data Structure (NVM Express 1.4+ spec, section 5.15.2)
    struct  __attribute__((packed)) IdentifyNamespaceData
    {
        uint64_t NSZE;        // Namespace Size, total LBAs
        uint64_t NCAP;        // Namespace Capacity, usable LBAs
        uint64_t NUSE;        // Namespace Utilization, currently used LBAs
        uint8_t  NSFEAT;      // Namespace Features
        uint8_t  NLBAF;       // Number of LBA Formats (0-based)
        uint8_t  FLBAS;       // Formatted LBA Size, bits[3:0] = active format index
        uint8_t  MC;          // Metadata Capabilities
        uint8_t  DPC;         // End-to-end Data Protection Capabilities
        uint8_t  DPS;         // End-to-end Data Protection Settings
        uint8_t  NMIC;        // Namespace Multi-path I/O and Namespace Sharing Capabilities
        uint8_t  RESCAP;      // Reservation Capabilities
        uint8_t  FPI;         // Format Progress Indicator
        uint8_t  DLFEAT;      // Deallocate Logical Block Features
        uint16_t NAWUN;       // Namespace Atomic Write Unit Normal
        uint16_t NAWUPF;      // Namespace Atomic Write Unit Power Fail
        uint16_t NACWU;       // Namespace Atomic Compare & Write Unit
        uint16_t NABSN;       // Namespace Atomic Boundary Size Normal
        uint16_t NABO;        // Namespace Atomic Boundary Offset
        uint16_t NABSPF;      // Namespace Atomic Boundary Size Power Fail
        uint16_t NOIOB;       // Namespace Optimal I/O Boundary
        uint8_t  NVMCAP[16];  // NVM Capacity in bytes (128-bit)
        uint8_t  reserved1[40];
        uint8_t  NGUID[16];   // Namespace Globally Unique Identifier
        uint64_t EUI64;       // IEEE Extended Unique Identifier
        
        // LBA Format descriptors, up to 16 entries
        struct LBAFormat
        {
            uint16_t MS;      // Metadata Size
            uint8_t  LBADS;   // LBA Data Size, sector = 2^LBADS bytes
            uint8_t  RP;      // Relative Performance (bits[1:0])
        } LBAF[16];           // bytes 128–191
        
        uint8_t reserved2[192];
        uint8_t vendorSpecific[3712];

    };

    struct __attribute__((packed)) CompletionQueueEntry
    {
        uint32_t result;
        uint32_t reserved;
        uint16_t sqHead;
        uint16_t sqID;
        uint16_t commandID;
        uint16_t status;
    };
    bool initialiseController();
    bool initialiseIdentity();
    bool initialiseAdminQueue();
    bool initialiseIOQueue();
    bool initialiseNamespaceData();
    void printNVMeInfo(uint64_t identity);
    void submitAdminCommand(SubmissionQueueEntry* entry);
    bool pollAdminCommandCompletion();
    void submitIOCommand(SubmissionQueueEntry* entry);
    bool pollIOCommandCompletion();
    bool initialise();

    namespace API
    {
        uint64_t getTotalSectors();
        uint64_t getSectorSize();
        uint64_t getUsableSectors();
        bool readSectors(uint64_t sectorStart, uint64_t sectorCount, uint64_t bufferPhysical);
        bool writeSectors(uint64_t sectorStart, uint64_t sectorCount, uint64_t bufferPhysical);
        bool zeroOutSectors(uint64_t sectorStart, uint64_t sectorCount);
    }
    namespace Test
    {
        bool testRound();
    }
}
