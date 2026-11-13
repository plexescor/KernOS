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

    struct __attribute__((packed)) CompletionQueueEntry
    {
        uint32_t result;
        uint32_t reserved;
        uint16_t sqHead;
        uint16_t sqID;
        uint16_t commandID;
        uint16_t status;
    };
    void printNVMeInfo(uint64_t identity);
    void submitAdminCommand(SubmissionQueueEntry* entry);
    bool pollAdminCommandCompletion(uint16_t commandID);
    bool initialise();
}
