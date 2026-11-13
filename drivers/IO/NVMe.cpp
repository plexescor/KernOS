#include "Drivers/IO/NVMe.hpp"
#include "Memory/PhysicalMemoryManager.hpp"
#include "Memory/VirtualMemoryManager.hpp"
#include "Libc/Memory.hpp"
#include "IO/PCI.hpp"
#include "TTY/TTY.hpp"
#include <cassert>
#include <cstdint>

uint64_t NVMEBAR0 = 0;

extern void uint64ToHex(uint64_t value, char* buffer);
extern const char* uint64ToString(uint64_t value, char* bufferOut, int bufferSize);
extern VirtualMemoryManager* vmm_G;
extern PhysicalMemoryManager* pmm_G;

// Register Offsets
#define CONTROLLER_CAPABILITIES      0x00
#define NVMe_VERSION                 0x08
#define CONTROLLER_CONFIGURATION     0x14
#define CONTROLLER_STATUS            0x1C
#define ADMIN_QUEUE_ATTRIBUTES       0x24
#define ADMIN_SUBMISSION_QUEUE_BASE  0x28
#define ADMIN_COMPLETION_QUEUE_BASE  0x30
#define DOORBELL                     0x1000

// CAP (Controller Capabilities) Bitmasks
#define MAX_QUEUE_ENTRIES_MASK           0xFFFFULL
#define TIMEOUT_MASK                     (0xFFULL << 24)
#define DOORBELL_STRIDE_MASK             (0xFULL << 32)
#define MINIMUM_SUPPORTED_PAGE_SIZE_MASK (0xFULL << 48)

// CC (Controller Configuration) Bitmasks
#define ENABLE_MASK                      (0x1U)
#define NVM_COMMAND_SET_MASK             (0x7U << 4)
#define MEMORY_PAGE_SIZE_MASK            (0xFU << 7)
#define ARBITRATION_MECHANISM_MASK       (0x7U << 11)
#define SHUTDOWN_NOTIFICATION_MASK       (0x3U << 14)
#define IO_SUBMISSION_QUEUE_SIZE_MASK    (0xFU << 16)
#define IO_COMPLETION_QUEUE_SIZE_MASK    (0xFU << 20)

// Globals
// CAP
uint64_t controllerCapabilities;
uint32_t maxQueueEntries;
uint8_t timeout;
uint8_t doorbellStride;
uint64_t minimumSupportedPageSize;

// CC
uint32_t controllerConfiguration;

// ADMIN QUEUES
// Addresses
uint64_t ASQPhysical;
uint64_t ACQPhysical;
uint64_t ASQVirtual;
uint64_t ACQVirtual;
// Identity buffer
uint64_t identity;
uint64_t identityNamespace;
// Queue state
uint32_t adminSQTail = 0;
uint32_t adminCQHead = 0;
uint16_t adminCQPhase = 1;
uint16_t adminCommandID = 0;

// IO QUEUES
uint64_t IOSQPhysical;
uint64_t IOCQPhysical;
uint64_t IOSQVirtual;
uint64_t IOCQVirtual;
// Queue state
uint32_t IOSQTail = 0;
uint32_t IOCQHead = 0;
uint16_t IOCQPhase = 1;
uint16_t IOCommandID = 0;

// Drive namespace info or whatever
uint64_t totalSectors;
uint8_t activeIndex;
uint64_t sectorSize;
uint16_t metadataSize; // usually is 0 in consumer ssd, which is what we;ll target
uint64_t usableSectors;
uint64_t totalBytes;
uint64_t totalUsableBytes;

template<typename T>
static T mmioRead(uint64_t base, uint64_t offset)
{
    return *reinterpret_cast<volatile T*>(base + offset);
}

template<typename T>
static void mmioWrite(uint64_t base, uint64_t offset, T value)
{
    *reinterpret_cast<volatile T*>(base + offset) = value;
}

bool Driver::IO::NVMe::initialiseNamespaceData()
{
    // Identifying the namespace
    // Get a new page for it
    identityNamespace = pmm_G->allocFrame();
    if (!identityNamespace) return false;
    uint64_t identityNamespaceVirt = vmm_G->physicalToVirtual(identityNamespace);

    vmm_G->mapPage(identityNamespaceVirt, identityNamespace, PAGE_CACHE_DISABLE | PAGE_NX | PAGE_PRESENT | PAGE_WRITABLE);
    Libc::Memory::kmemset((void*)identityNamespaceVirt, 0, 4096);

    SubmissionQueueEntry identifyNamespaceEntry =
    {
        .opcode = 0x06,
        .flags = 0x00,
        .commandID = ++adminCommandID,
        .NSID = 0x01,
        .reserved = 0x00,
        .MPTR = 0x00,
        .PRP1 = identityNamespace,
        .PRP2 = 0x00,
        .CDW10 = 0x00,
        .CDW11 = 0x00,
        .CDW12 = 0x00,
        .CDW13 = 0x00,
        .CDW14 = 0x00,
        .CDW15 = 0x00
    };

    Driver::IO::NVMe::submitAdminCommand(&identifyNamespaceEntry);
    if (!Driver::IO::NVMe::pollAdminCommandCompletion())
    {
        ::TTY::putString("[FAIL] ", ::TTY::Color::RED);
        ::TTY::putString("NVMe Namespace Identify command failed!\n", ::TTY::Color::WHITE);
        return false;
    }

    char bufTotalSectors[21];
    char bufActiveIndex[21];
    char bufSectorSize[21];
    char bufMetadataSize[21];
    char bufUsableSectors[21];
    char bufTotalBytes[21];
    char bufTotalUsableBytes[21];

    IdentifyNamespaceData* data = reinterpret_cast<IdentifyNamespaceData*>(identityNamespaceVirt);

    totalSectors = data->NSZE;
    if (totalSectors == 0)
    {
        ::TTY::putString("[FAIL] ", TTY::Color::RED);
        ::TTY::putString("NSZE is 0 invalid namespace!\n", TTY::Color::WHITE);
        return false;
    }
    ::TTY::putString("[INFO] ", TTY::Color::CYAN);
    ::TTY::putString("Total Sectors (NSZE): ", TTY::Color::WHITE);
    ::TTY::putString(uint64ToString(totalSectors, bufTotalSectors, 21), TTY::Color::YELLOW);
    ::TTY::putString("\n", TTY::Color::WHITE);

    activeIndex = data->FLBAS & 0x0F;
    ::TTY::putString("[INFO] ", TTY::Color::CYAN);
    ::TTY::putString("Active LBA Format Index (FLBAS): ", TTY::Color::WHITE);
    ::TTY::putString(uint64ToString(activeIndex, bufActiveIndex, 21), TTY::Color::YELLOW);
    ::TTY::putString("\n", TTY::Color::WHITE);

    uint8_t lbads = data->LBAF[activeIndex].LBADS;
    if (lbads < 9 || lbads > 12)
    {
        ::TTY::putString("[FAIL] ", TTY::Color::RED);
        ::TTY::putString("Unsupported LBA data size (LBADS out of range)!\n", TTY::Color::WHITE);
        return false;
    }
    sectorSize = 1ULL << lbads;
    ::TTY::putString("[INFO] ", TTY::Color::CYAN);
    ::TTY::putString("Sector Size (bytes): ", TTY::Color::WHITE);
    ::TTY::putString(uint64ToString(sectorSize, bufSectorSize, 21), TTY::Color::YELLOW);
    ::TTY::putString("\n", TTY::Color::WHITE);

    metadataSize = data->LBAF[activeIndex].MS;
    if (metadataSize != 0)
    {
        ::TTY::putString("[FAIL] ", TTY::Color::RED);
        ::TTY::putString("Metadata sectors not supported!\n", TTY::Color::WHITE);
        return false;
    }
    ::TTY::putString("[INFO] ", TTY::Color::CYAN);
    ::TTY::putString("Metadata Size (should be 0): ", TTY::Color::WHITE);
    ::TTY::putString(uint64ToString(metadataSize, bufMetadataSize, 21), TTY::Color::YELLOW);
    ::TTY::putString("\n", TTY::Color::WHITE);

    usableSectors = data->NCAP;
    totalBytes = totalSectors * sectorSize;
    totalUsableBytes = usableSectors * sectorSize;

    ::TTY::putString("[INFO] ", TTY::Color::CYAN);
    ::TTY::putString("Usable Sectors (NCAP): ", TTY::Color::WHITE);
    ::TTY::putString(uint64ToString(usableSectors, bufUsableSectors, 21), TTY::Color::YELLOW);
    ::TTY::putString("\n", TTY::Color::WHITE);

    ::TTY::putString("[INFO] ", TTY::Color::CYAN);
    ::TTY::putString("Total Bytes: ", TTY::Color::WHITE);
    ::TTY::putString(uint64ToString(totalBytes, bufTotalBytes, 21), TTY::Color::YELLOW);
    ::TTY::putString("\n", TTY::Color::WHITE);

    ::TTY::putString("[INFO] ", TTY::Color::CYAN);
    ::TTY::putString("Total Usable Bytes: ", TTY::Color::WHITE);
    ::TTY::putString(uint64ToString(totalUsableBytes, bufTotalUsableBytes, 21), TTY::Color::YELLOW);
    ::TTY::putString("\n", TTY::Color::WHITE);

    return true;
}

void Driver::IO::NVMe::submitAdminCommand(SubmissionQueueEntry* entry)
{
    SubmissionQueueEntry* sq = reinterpret_cast<SubmissionQueueEntry*>(ASQVirtual);
    sq[adminSQTail] = *entry;

    adminSQTail = (adminSQTail + 1) % 64;

    // Notify the controller shit changed
    uint32_t sqDoorbellOffset = DOORBELL + (2 * 0) * (4 << doorbellStride);
    mmioWrite<uint32_t>(NVMEBAR0, sqDoorbellOffset, adminSQTail);
}

bool Driver::IO::NVMe::pollAdminCommandCompletion()
{
    volatile CompletionQueueEntry* cq = reinterpret_cast<volatile CompletionQueueEntry*>(ACQVirtual);
    while (true)
    {
        // TODO add a timeout here too
        volatile CompletionQueueEntry* entry = &cq[adminCQHead];
        if ((entry->status & 0x1) == adminCQPhase)
        {
            // Got a completion
            uint16_t statusCode = (entry->status >> 1) & 0xFF;

            // Advance head
            adminCQHead = (adminCQHead + 1) % 64;
            if (adminCQHead == 0) adminCQPhase ^= 1; // wrapped, flip phase

            // Ring CQ doorbell
            uint32_t cqDoorbellOffset = DOORBELL + (2 * 0 + 1) * (4 << doorbellStride);
            mmioWrite<uint32_t>(NVMEBAR0, cqDoorbellOffset, adminCQHead);

            return statusCode == 0; // 0 = success
        }
    }

}


void Driver::IO::NVMe::submitIOCommand(SubmissionQueueEntry* entry)
{
    SubmissionQueueEntry* sq = reinterpret_cast<SubmissionQueueEntry*>(IOSQVirtual);
    sq[IOSQTail] = *entry;

    IOSQTail = (IOSQTail + 1) % 64;

    // Notify the controller shit changed
    uint32_t sqDoorbellOffset = DOORBELL + (2 * 1) * (4 << doorbellStride);
    mmioWrite<uint32_t>(NVMEBAR0, sqDoorbellOffset, IOSQTail);
}

bool Driver::IO::NVMe::pollIOCommandCompletion()
{
    volatile CompletionQueueEntry* cq = reinterpret_cast<volatile CompletionQueueEntry*>(IOCQVirtual);
    while (true)
    {
        // TODO add a timeout here too
        volatile CompletionQueueEntry* entry = &cq[IOCQHead];
        if ((entry->status & 0x1) == IOCQPhase)
        {
            // Got a completion
            uint16_t statusCode = (entry->status >> 1) & 0xFF;

            // Advance head
            IOCQHead = (IOCQHead + 1) % 64;
            if (IOCQHead == 0) IOCQPhase ^= 1; // wrapped, flip phase

            // Ring CQ doorbell
            uint32_t cqDoorbellOffset = DOORBELL + (2 * 1 + 1) * (4 << doorbellStride);
            mmioWrite<uint32_t>(NVMEBAR0, cqDoorbellOffset, IOCQHead);

            return statusCode == 0; // 0 = success
        }
    }

}

void Driver::IO::NVMe::printNVMeInfo(uint64_t identity)
{
    uint8_t* raw = reinterpret_cast<uint8_t*>(identity);

    char model[41];
    for (int i = 0; i < 40; i++)
    {
        char c = (char)raw[24 + i];
        model[i] = (c == '\0') ? ' ' : c; // replace nulls with spaces
    }
    model[40] = '\0';

    // trim trailing spaces
    for (int i = 39; i >= 0 && model[i] == ' '; i--)
        model[i] = '\0';

    ::TTY::putString("[INFO] ", TTY::Color::CYAN);
    ::TTY::putString(model, ::TTY::Color::WHITE);
    ::TTY::putString("\n", TTY::Color::WHITE);
}

bool Driver::IO::NVMe::initialise()
{
    if (!vmm_G) return false;
    
    if (!Driver::IO::NVMe::initialiseController()) return false;
    if (!Driver::IO::NVMe::initialiseAdminQueue()) return false;
    if (!Driver::IO::NVMe::initialiseIdentity()) return false;
    if (!Driver::IO::NVMe::initialiseNamespaceData()) return false;
    if (!Driver::IO::NVMe::initialiseIOQueue()) return false;



    ::TTY::putString("[ OK ] ", ::TTY::Color::LIME);
    ::TTY::putString("NVMe Initialisation successful!\n", ::TTY::Color::WHITE);
    return true;
}

bool Driver::IO::NVMe::initialiseController()
{
    NVMEBAR0 = ::IO::PCI::getBAR0_NVME();
    char buffer[21];
    uint64ToHex(NVMEBAR0, buffer);
    ::TTY::putString("[TEST] ", ::TTY::Color::CYAN);
    ::TTY::putString("NVME BAR0 Address Hex: ", ::TTY::Color::WHITE);
    ::TTY::putString(buffer, ::TTY::Color::PURPLE);
    ::TTY::putString("\n", TTY::Color::WHITE);

    vmm_G->mapPage(NVMEBAR0, NVMEBAR0, PAGE_PRESENT | PAGE_WRITABLE | PAGE_NX | PAGE_CACHE_DISABLE);

    controllerCapabilities = *(volatile uint64_t*)(NVMEBAR0 + CONTROLLER_CAPABILITIES);

    maxQueueEntries = (uint32_t)(controllerCapabilities & MAX_QUEUE_ENTRIES_MASK) + 1;
    timeout = (uint8_t)((controllerCapabilities & TIMEOUT_MASK) >> 24);
    doorbellStride = (uint8_t)((controllerCapabilities & DOORBELL_STRIDE_MASK) >> 32);

    uint64_t mpsMin = (controllerCapabilities & MINIMUM_SUPPORTED_PAGE_SIZE_MASK) >> 48;
    minimumSupportedPageSize = 1ULL << (12 + mpsMin);

    // Clear the buffer

    ::TTY::putString("[TEST] ", ::TTY::Color::CYAN);
    const char* minSizeStr = uint64ToString(minimumSupportedPageSize, buffer, 21);
    ::TTY::putString("Minimum page size supported by the NVMe: ", ::TTY::Color::WHITE);
    ::TTY::putString(minSizeStr, ::TTY::Color::YELLOW);
    ::TTY::putString("\n", ::TTY::Color::WHITE);

    // we will only map in 4096 per page for now
    if (minimumSupportedPageSize > 4096)
    {
        ::TTY::putString("[FAIL] ", ::TTY::Color::RED);
        ::TTY::putString("NVMe Controller doesn't support minimum page size (4096)!\n", ::TTY::Color::WHITE);
        return false;
    }

    uint64_t pagesToMap = 4;
    // 1 because we have already mapped the NVMEBAR0 itself
    for (uint64_t page = 1; page < pagesToMap; page++)
    {
        uint64_t physicalAddress = NVMEBAR0 + page * 4096;
        vmm_G->mapPage(physicalAddress, physicalAddress, PAGE_PRESENT | PAGE_WRITABLE | PAGE_NX | PAGE_CACHE_DISABLE);
    }

    // Now disable the controller
    controllerConfiguration = *(volatile uint32_t*)(NVMEBAR0 + CONTROLLER_CONFIGURATION);
    uint32_t CCDisable = controllerConfiguration & ~ENABLE_MASK;
    *(volatile uint32_t*)(NVMEBAR0 + CONTROLLER_CONFIGURATION) = CCDisable;

    ::TTY::putString("[INIT] ", ::TTY::Color::CYAN);
    ::TTY::putString("Waiting for NVMe controller to reset...\n", ::TTY::Color::WHITE);
    // Wait until controller resets (0)
    while (*(volatile uint32_t*)(NVMEBAR0 + CONTROLLER_STATUS) & 0x1)
    {
        // TODO add a timeoout
    }
    ::TTY::putString("[ OK ] ", ::TTY::Color::LIME);
    ::TTY::putString("NVMe Controller reset successfully...\n", ::TTY::Color::WHITE);
    return true;
}

bool Driver::IO::NVMe::initialiseAdminQueue()
{
    // Now allocate two pages for ASQ, and ACQ
    ASQPhysical = pmm_G->allocFrame();
    ACQPhysical = pmm_G->allocFrame();

    vmm_G->mapPage(ASQPhysical, ASQPhysical, PAGE_WRITABLE | PAGE_PRESENT | PAGE_NX | PAGE_CACHE_DISABLE);
    vmm_G->mapPage(ACQPhysical, ACQPhysical, PAGE_WRITABLE | PAGE_PRESENT | PAGE_NX | PAGE_CACHE_DISABLE);
    ASQVirtual = vmm_G->physicalToVirtual(ASQPhysical);
    ACQVirtual = vmm_G->physicalToVirtual(ACQPhysical);

    Libc::Memory::kmemset((void*)ASQVirtual, 0, 4096);
    Libc::Memory::kmemset((void*)ACQVirtual, 0, 4096);

    mmioWrite<uint32_t>(NVMEBAR0, ADMIN_QUEUE_ATTRIBUTES, (63 << 0) | (63 << 16));
    mmioWrite<uint64_t>(NVMEBAR0, ADMIN_SUBMISSION_QUEUE_BASE, ASQPhysical);
    mmioWrite<uint64_t>(NVMEBAR0, ADMIN_COMPLETION_QUEUE_BASE, ACQPhysical);

    // Now configuring the CC
    uint32_t cc = 0;
    cc |= (0 << 0);   // EN=0
    cc |= (0 << 4);   // CSS=0 (NVM command set)
    cc |= (0 << 7);   // MPS=0 (4KB pages)
    cc |= (6 << 16);  // IOSQES=6 (64B)
    cc |= (4 << 20);
    mmioWrite<uint32_t>(NVMEBAR0, CONTROLLER_CONFIGURATION, cc);

    // Enable the EN
    cc |= (1 << 0);
    mmioWrite<uint32_t>(NVMEBAR0, CONTROLLER_CONFIGURATION, cc);

    ::TTY::putString("[INIT] ", ::TTY::Color::CYAN);
    ::TTY::putString("Waiting for NVMe controller to initialise...\n", ::TTY::Color::WHITE);
    // Wait until controller initialises (1)
    while (true)
    {
        uint32_t csts = *(volatile uint32_t*)(NVMEBAR0 + CONTROLLER_STATUS);

        if (csts & 0x2) // CFS = fatal error
        {
            ::TTY::putString("[FAIL] ", ::TTY::Color::RED);
            ::TTY::putString("NVMe Controller fatal error!\n", ::TTY::Color::WHITE);
            return false;
        }

        if (csts & 0x1) break; // RDY = ready
    }
    ::TTY::putString("[ OK ] ", ::TTY::Color::LIME);
    ::TTY::putString("NVMe Controller initialised successfully...\n", ::TTY::Color::WHITE);

    return true;
}

bool Driver::IO::NVMe::initialiseIdentity()
{
    // Now we get info from the NVMe
    identity = pmm_G->allocFrame();
    if (!identity) return false;
    if (identity & 0xFFF)
    {
        ::TTY::putString("[FAIL] ", ::TTY::Color::RED);
        ::TTY::putString("Identity buffer not page aligned!\n", ::TTY::Color::WHITE);
        return false;
    }
    uint64_t identityVirt = vmm_G->physicalToVirtual(identity);

    vmm_G->mapPage(identityVirt, identity, PAGE_CACHE_DISABLE | PAGE_NX | PAGE_PRESENT | PAGE_WRITABLE);
    Libc::Memory::kmemset((void*)identityVirt, 0, 4096);
    
    SubmissionQueueEntry identifyEntry =
    {
        .opcode = 0x06,
        .flags = 0x00,
        .commandID = ++adminCommandID, 
        .NSID = 0x00,
        .reserved = 0x00,
        .MPTR = 0x00,
        .PRP1 = identity,
        .PRP2 = 0x00,
        .CDW10 = 0x00000001,
        .CDW11 = 0x00,
        .CDW12 = 0x00,
        .CDW13 = 0x00,
        .CDW14 = 0x00,
        .CDW15 = 0x00
    };

    Driver::IO::NVMe::submitAdminCommand(&identifyEntry);
    if (!Driver::IO::NVMe::pollAdminCommandCompletion())
    {
        ::TTY::putString("[FAIL] ", ::TTY::Color::RED);
        ::TTY::putString("NVMe Identify command failed!\n", ::TTY::Color::WHITE);
        return false;
    }
    // uint8_t* raw = reinterpret_cast<uint8_t*>(identityVirt);
    // char model[41];
    // for (int i = 0; i < 40; i++)
    //     model[i] = (char)raw[24 + i];
    // model[40] = '\0';
    // ::TTY::putString(model, TTY::Color::WHITE);
    // ::TTY::putString("\n", ::TTY::Color::WHITE);

    Driver::IO::NVMe::printNVMeInfo(identityVirt);
    return true;
}

bool Driver::IO::NVMe::initialiseIOQueue()
{
    IOCQPhysical = pmm_G->allocFrame();
    IOSQPhysical = pmm_G->allocFrame();
    if (!IOCQPhysical || !IOSQPhysical) return false;
    
    vmm_G->mapPage(IOCQPhysical, IOCQPhysical, PAGE_CACHE_DISABLE | PAGE_NX | PAGE_PRESENT | PAGE_WRITABLE);
    vmm_G->mapPage(IOSQPhysical, IOSQPhysical, PAGE_CACHE_DISABLE | PAGE_NX | PAGE_PRESENT | PAGE_WRITABLE);
    IOCQVirtual = vmm_G->physicalToVirtual(IOCQPhysical);
    IOSQVirtual = vmm_G->physicalToVirtual(IOSQPhysical);

    Libc::Memory::kmemset((void*)IOCQVirtual, 0, 4096);
    Libc::Memory::kmemset((void*)IOSQVirtual, 0, 4096);

    SubmissionQueueEntry createCQEntry =
    {
        .opcode    = 0x05,
        .flags     = 0x00,
        .commandID = ++adminCommandID,
        .NSID      = 0x00,
        .reserved  = 0x00,
        .MPTR      = 0x00,
        .PRP1      = IOCQPhysical,
        .PRP2      = 0x00,
        .CDW10     = (63 << 16) | 1,
        .CDW11     = 0x1,
        .CDW12     = 0x00,
        .CDW13     = 0x00,
        .CDW14     = 0x00,
        .CDW15     = 0x00
    };

    SubmissionQueueEntry createSQEntry =
    {
        .opcode    = 0x01,
        .flags     = 0x00,
        .commandID = ++adminCommandID,
        .NSID      = 0x00,
        .reserved  = 0x00,
        .MPTR      = 0x00,
        .PRP1      = IOSQPhysical, 
        .PRP2      = 0x00,
        .CDW10     = (63 << 16) | 1,
        .CDW11     = (1 << 16) | 0x1,
        .CDW12     = 0x00,
        .CDW13     = 0x00,
        .CDW14     = 0x00,
        .CDW15     = 0x00
    };

    submitAdminCommand(&createCQEntry);
    if (!pollAdminCommandCompletion()) return false;

    submitAdminCommand(&createSQEntry);
    if (!pollAdminCommandCompletion()) return false;

    return true;
}

// Now the actual API ~everyone~ will use
// Make sure the page you give is physical and zeroed out!!!
bool Driver::IO::NVMe::API::readSectors(uint64_t sectorStart, uint64_t sectorCount, uint64_t bufferPhysical)
{
    SubmissionQueueEntry sqEntry =
    {
        .opcode    = 0x02, // Read
        .flags     = 0x00,
        .commandID = ++IOCommandID,
        .NSID      = 0x01,
        .reserved  = 0x00,
        .MPTR      = 0x00,
        .PRP1      = bufferPhysical,
        .PRP2      = 0x00,
        .CDW10     = (uint32_t)(sectorStart & 0xFFFFFFFF),
        .CDW11     = (uint32_t)(sectorStart >> 32),
        .CDW12     = (uint32_t)(sectorCount - 1),
        .CDW13     = 0x00,
        .CDW14     = 0x00,
        .CDW15     = 0x00
    };

    Driver::IO::NVMe::submitIOCommand(&sqEntry);
    if (!pollIOCommandCompletion()) return false;
    return true;
}

// FIXED, sometimes the NSZE is 0 (totalSectors) causing init to fail
// FIXED, sometimes readSector hangs
// The above shit was fixed by adding fucking volatile fucking flag fuck you compiler
// ^__^
// There may be other b_u_g_s which i havent found
// i wrote b_u_g_s like that so searching the codebase for that string will only bring out genuine issues
// and not generic strings which contain the string being searched for

// Make sure the page you give is physical and zeroed out!!!
bool Driver::IO::NVMe::API::writeSectors(uint64_t sectorStart, uint64_t sectorCount, uint64_t bufferPhysical)
{
    SubmissionQueueEntry sqEntry =
    {
        .opcode    = 0x01,// Write
        .flags     = 0x00,
        .commandID = ++IOCommandID,
        .NSID      = 0x01,
        .reserved  = 0x00,
        .MPTR      = 0x00,
        .PRP1      = bufferPhysical,
        .PRP2      = 0x00,
        .CDW10     = (uint32_t)(sectorStart & 0xFFFFFFFF),
        .CDW11     = (uint32_t)(sectorStart >> 32),
        .CDW12     = (uint32_t)(sectorCount - 1),
        .CDW13     = 0x00,
        .CDW14     = 0x00,
        .CDW15     = 0x00
    };

    Driver::IO::NVMe::submitIOCommand(&sqEntry);
    if (!pollIOCommandCompletion()) return false;
    return true;
}

bool Driver::IO::NVMe::Test::testRound()
{
    uint64_t commonBuffer = pmm_G->allocFrame();
    if (!commonBuffer) 
    {
        ::TTY::putString("[FAIL] ", ::TTY::Color::RED);
        ::TTY::putString("Could not allocate a free frame! OOM?\n", TTY::Color::WHITE);
    }
    uint64_t virtualBuffer = vmm_G->physicalToVirtual(commonBuffer);
    vmm_G->mapPage(virtualBuffer, commonBuffer, PAGE_NX | PAGE_PRESENT | PAGE_WRITABLE);

    Libc::Memory::kmemset((void*)virtualBuffer, 0, 4096);

    uint8_t* vbuf = reinterpret_cast<uint8_t*>(virtualBuffer);

    vbuf[0] = 'h';
    vbuf[1] = 'e';
    vbuf[2] = 'l';
    vbuf[3] = 'l';
    vbuf[4] = 'o';

    // ::TTY::putString("[TEST] Before write\n", TTY::Color::WHITE);
    if(!Driver::IO::NVMe::API::writeSectors(1, 1, commonBuffer))
    {
        ::TTY::putString("[FAIL] ", ::TTY::Color::RED);
        ::TTY::putString("Unknown error while writing sectors to disk!\n", TTY::Color::WHITE);
    }
    // ::TTY::putString("[TEST] After write\n", TTY::Color::WHITE);

    // Libc::Memory::kmemset((void*)virtualBuffer, 0, 4096);

    uint64_t commonBuffer2 = pmm_G->allocFrame();
    if (!commonBuffer2) 
    {
        ::TTY::putString("[FAIL] ", ::TTY::Color::RED);
        ::TTY::putString("Could not allocate a free frame! OOM?\n", TTY::Color::WHITE);
    }
    uint64_t virtualBuffer2 = vmm_G->physicalToVirtual(commonBuffer2);
    vmm_G->mapPage(virtualBuffer2, commonBuffer2, PAGE_NX | PAGE_PRESENT | PAGE_WRITABLE);

    Libc::Memory::kmemset((void*)virtualBuffer2, 0, 4096);

    uint8_t* vbuf2 = reinterpret_cast<uint8_t*>(virtualBuffer2);


    if (!Driver::IO::NVMe::API::readSectors(1, 1, commonBuffer2))
    {
        ::TTY::putString("[FAIL] ", ::TTY::Color::RED);
        ::TTY::putString("Error while reading sectors from disk!\n", TTY::Color::WHITE);
    }

    if (vbuf2[0] == 'h' && vbuf2[1] == 'e' && vbuf2[2] == 'l' && vbuf2[3] == 'l' && vbuf2[4] == 'o')
    {
        ::TTY::putString("[PASS] ", ::TTY::Color::LIME);
        ::TTY::putString("Disk round test passed!\n", TTY::Color::WHITE);
        return true;
    }
    return false;
}