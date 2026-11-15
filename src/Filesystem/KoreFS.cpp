#include "Filesystem/KoreFS.hpp"
#include "Memory/VirtualMemoryManager.hpp"
#include "TTY/TTY.hpp"
#include "Memory/PhysicalMemoryManager.hpp"
#include "Libc/Memory.hpp"

#include "Drivers/IO/NVMe.hpp" // May need to make an abstraction
#include <cstdint>
#include <optional>


uint64_t RESERVED_FREE_REGION_SECTORS = 0;
// TODO maybe keep sectorSize in mind to write data, its 512 on qemu so i guess we will do it later

// BEWARE all space usage, free space have the unit 'bytes';

// FUTURE may need to update the starting LBA of our header to be compliant with the one 
// told by the GPT header

extern PhysicalMemoryManager* pmm_G;
extern VirtualMemoryManager* vmm_G;
extern void uint64ToHex(uint64_t value, char* buffer);
extern const char* uint64ToString(uint64_t value, char* bufferOut, int bufferSize);


bool Filesystem::KoreFS::driveHeaderExists()
{
    std::optional<DriveHeader> header = readDriveheader();
    if (!header.has_value())
    {
        ::TTY::putString("[KOREFS] Failed to read DriveHeader.\n", ::TTY::Color::RED);
        return false;
    }

    // Safely null-terminate the 8-byte magic array before printing
    char magicStr[9];
    Libc::Memory::kmemcpy(magicStr, header->magic, 8);
    magicStr[8] = '\0';

    char numBuf[24];

    ::TTY::putString("[KOREFS] Magic:      \"", ::TTY::Color::WHITE);
    ::TTY::putString(magicStr, ::TTY::Color::GREEN);
    ::TTY::putString("\"\n", ::TTY::Color::WHITE);

    ::TTY::putString("[KOREFS] SectorSize: ", ::TTY::Color::WHITE);
    ::TTY::putString(uint64ToString(header->sectorSize, numBuf, sizeof(numBuf)), ::TTY::Color::WHITE);
    ::TTY::putString("\n", ::TTY::Color::WHITE);

    ::TTY::putString("[KOREFS] DriveSize:  ", ::TTY::Color::WHITE);
    ::TTY::putString(uint64ToString(header->driveSize, numBuf, sizeof(numBuf)), ::TTY::Color::WHITE);
    ::TTY::putString("\n", ::TTY::Color::WHITE);

    ::TTY::putString("[KOREFS] UsedSpace:  ", ::TTY::Color::WHITE);
    ::TTY::putString(uint64ToString(header->usedSpace, numBuf, sizeof(numBuf)), ::TTY::Color::WHITE);
    ::TTY::putString("\n", ::TTY::Color::WHITE);

    ::TTY::putString("[KOREFS] FreeSpace:  ", ::TTY::Color::WHITE);
    ::TTY::putString(uint64ToString(header->freeSpace, numBuf, sizeof(numBuf)), ::TTY::Color::WHITE);
    ::TTY::putString("\n", ::TTY::Color::WHITE);

    // Verify magic matches "KOREFS  " before confirming the header exists
    const char expectedMagic[8] = {'K', 'O', 'R', 'E', 'F', 'S', ' ', ' '};
    for (size_t i = 0; i < 8; i++)
    {
        if (header->magic[i] != expectedMagic[i]) return false;
    }

    return true;
}


bool Filesystem::KoreFS::initialise()
{
    if (!driveHeaderExists())
    {
        if (!writeDriveHeader()) return false;
        return true;
    }
    TTY::putString("[INFO] ", TTY::Color::CYAN);
    TTY::putString("Drive header exists. Continuing...\n", TTY::Color::WHITE);
    return true;
}

bool Filesystem::KoreFS::writeDriveHeader()
{
    TTY::putString("[INIT] ", TTY::Color::CYAN);
    TTY::putString("Drive header doesnt exist, writing it...\n", TTY::Color::WHITE);

    uint64_t totalSectors = Driver::IO::NVMe::API::getTotalSectors();
    uint64_t sectorSize = Driver::IO::NVMe::API::getSectorSize();

    // Reserve some sectors for our freeRegion tracking
    // 1GB is plenty.. (67 million btw)
    uint64_t reservedRegionsNumber = (1 * 1024 * 1024 * 1024) / sectorSize;
    RESERVED_FREE_REGION_SECTORS = reservedRegionsNumber;
    uint64_t rootFolderHeaderStart = 2049 + reservedRegionsNumber;

    uint64_t freeRegionStart = rootFolderHeaderStart + 1;
    uint64_t freeRegionSectorCount = totalSectors - freeRegionStart;

    FreeRegion freeRegion = 
    { 
        .startSector = freeRegionStart, 
        .sectorSpan = freeRegionSectorCount 
    };

    DriveHeader header = 
    {
        // FUTURE may need to bump shit to accomodate for an EFI partition as boootloaders are heavy....
        // FUTURE may need to keep in account usableSectors as some maybe reserved for the gpt header i guess..
        .magic = {'K', 'O', 'R', 'E', 'F', 'S', ' ', ' '},
        .sectorSize = sectorSize,
        .driveSize = totalSectors * sectorSize,
        .usedSpace = freeRegionStart * sectorSize,
        .freeSpace = freeRegionSectorCount * sectorSize, 
        .rootFolderHeaderSector = rootFolderHeaderStart,
        .freeRegionCount = 1
        // Free region starts just after this struct
    };

    uint64_t buffer = pmm_G->allocFrame();
    if (!buffer)
    {
        ::TTY::putString("[FAIL] ", ::TTY::Color::RED);
        ::TTY::putString("Could not allocate a free frame! OOM?\n", TTY::Color::WHITE);
        return false;
    }
    uint64_t bufferVirtual = vmm_G->physicalToVirtual(buffer);
    vmm_G->mapPage(bufferVirtual, buffer, PAGE_NX | PAGE_PRESENT | PAGE_WRITABLE);
    Libc::Memory::kmemset((void*)bufferVirtual, 0, 4096);

    Libc::Memory::kmemcpy((void*)bufferVirtual, (void*)&header, sizeof(header));

    Driver::IO::NVMe::API::writeSectors(2048, 1, buffer);
    
    Filesystem::KoreFS::writeFreeRegion(&freeRegion, 2048);
    pmm_G->freeFrame(buffer);
    vmm_G->unMapPage(bufferVirtual);
    return true;
}

bool Filesystem::KoreFS::writeFreeRegion(FreeRegion *freeRegion, uint64_t driveHeaderStart)
{
    bool foundEmptySpace = false;
    uint64_t offset = driveHeaderStart + 1;
    uint64_t sectorSize = Driver::IO::NVMe::API::getSectorSize();
    uint64_t entriesPerSector = sectorSize / sizeof(FreeRegion);

    uint64_t buffer = pmm_G->allocFrame();
    if (!buffer)
    {
        ::TTY::putString("[FAIL] ", ::TTY::Color::RED);
        ::TTY::putString("Could not allocate a free frame! OOM?\n", TTY::Color::WHITE);
        return false;
    }
    uint64_t bufferVirtual = vmm_G->physicalToVirtual(buffer);
    vmm_G->mapPage(bufferVirtual, buffer, PAGE_NX | PAGE_PRESENT | PAGE_WRITABLE);

    for (uint64_t currentSector = 0; currentSector < RESERVED_FREE_REGION_SECTORS; currentSector++)
    {
        Libc::Memory::kmemset((void*)bufferVirtual, 0, 4096);
        if (!Driver::IO::NVMe::API::readSectors(offset + currentSector, 1, buffer))
        {
            ::TTY::putString("[FAIL] ", TTY::Color::RED);
            ::TTY::putString("Unknown error while reading sectors from disk!\n", TTY::Color::WHITE);
            pmm_G->freeFrame(buffer);
            vmm_G->unMapPage(bufferVirtual);
            return false;
        }

        FreeRegion* regions = reinterpret_cast<FreeRegion*>(bufferVirtual);
        for (uint64_t i = 0; i < entriesPerSector; i++)
        {
            // When we allocate something and erase from freeRegion, we need to clear both to 0
            if (regions[i].sectorSpan == 0 && regions[i].sectorSpan == 0) // We found empty slot
            {
                regions[i] = *freeRegion;

                if (!Driver::IO::NVMe::API::writeSectors(offset + currentSector, 1, buffer))
                {
                    ::TTY::putString("[FAIL] ", TTY::Color::RED);
                    ::TTY::putString("Unknown error while writing sectors to disk!\n", TTY::Color::WHITE);
                    pmm_G->freeFrame(buffer);
                    vmm_G->unMapPage(bufferVirtual);
                    return false;
                }
                pmm_G->freeFrame(buffer);
                vmm_G->unMapPage(bufferVirtual);
                return true;
            }
        }
    }

    ::TTY::putString("[FAIL] FreeRegion table full!\n", TTY::Color::RED);
    pmm_G->freeFrame(buffer);
    vmm_G->unMapPage(bufferVirtual);
    return true;
}

std::optional<Filesystem::KoreFS::DriveHeader> Filesystem::KoreFS::readDriveheader()
{
    uint64_t sectorSize = Driver::IO::NVMe::API::getSectorSize();
    RESERVED_FREE_REGION_SECTORS = (1ULL * 1024 * 1024 * 1024) / sectorSize;
    
    uint64_t buffer = pmm_G->allocFrame();
    
    if (!buffer)
    {
        ::TTY::putString("[FAIL] ", ::TTY::Color::RED);
        ::TTY::putString("Could not allocate a free frame! OOM?\n", TTY::Color::WHITE);
        return std::nullopt;
    }
    uint64_t bufferVirtual = vmm_G->physicalToVirtual(buffer);
    vmm_G->mapPage(bufferVirtual, buffer, PAGE_NX | PAGE_PRESENT | PAGE_WRITABLE);
    Libc::Memory::kmemset((void*)bufferVirtual, 0, 4096);

    // Now read the sectors from the drive
    if (!Driver::IO::NVMe::API::readSectors(2048, 1, buffer))
    {
        ::TTY::putString("[FAIL] ", TTY::Color::RED);
        ::TTY::putString("Unknown error while reading sectors from disk!\n", TTY::Color::WHITE);
        pmm_G->freeFrame(buffer);
        vmm_G->unMapPage(bufferVirtual);
        return std::nullopt;
    }

    Filesystem::KoreFS::DriveHeader header = {};

    Libc::Memory::kmemcpy((void*)&header, (void*)bufferVirtual, sizeof(header));

    pmm_G->freeFrame(buffer);
    vmm_G->unMapPage(bufferVirtual);
    return header;
}