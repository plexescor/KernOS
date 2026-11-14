#include "Filesystem/KoreFS.hpp"
#include "Memory/VirtualMemoryManager.hpp"
#include "TTY/TTY.hpp"
#include "Memory/PhysicalMemoryManager.hpp"
#include "Libc/Memory.hpp"

#include "Drivers/IO/NVMe.hpp" // May need to make an abstraction
#include <cstdint>
#include <optional>

// TODO maybe keep sectorSize in mind to write data, its 512 on qemu so i guess we will do it later

// BEWARE all space usage, free space have the unit number of sectors not bytes;

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
    ::TTY::putString(uint64ToString(header->freespace, numBuf, sizeof(numBuf)), ::TTY::Color::WHITE);
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
    uint64_t usableSectors = Driver::IO::NVMe::API::getUsableSectors();

    if (totalSectors != usableSectors)
    {
        ::TTY::putString("[FAIL] ", TTY::Color::RED);
        ::TTY::putString("Total sectors and usable sectors dont match!\n", TTY::Color::WHITE);
        return false;
    }

    DriveHeader header = 
    {
        // FUTURE may need to bump shit to accomodate for an EFI partition as boootloaders are heavy....
        .magic = {'K', 'O', 'R', 'E', 'F', 'S', ' ', ' '},
        .sectorSize = sectorSize,
        .driveSize = totalSectors,
        .usedSpace = 1 * 2, // We can assume that when we write this header, (during formatting), (only LBA0 and LBA will be used)
                            // By the Bootloader or whatever (Bootloader will need more space i guess) and the header itself
        .freespace = totalSectors - 1 * 2 // same deal, accounting for the header of bootloader
        // I can be wrong tho we will see
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

    Driver::IO::NVMe::API::writeSectors(1, 1, buffer);
    
    return true;
}

std::optional<Filesystem::KoreFS::DriveHeader> Filesystem::KoreFS::readDriveheader()
{
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
    if (!Driver::IO::NVMe::API::readSectors(1, 1, buffer))
    {
        ::TTY::putString("[FAIL] ", TTY::Color::RED);
        ::TTY::putString("Unknown error while reading sectors from disk!\n", TTY::Color::WHITE);
    }

    Filesystem::KoreFS::DriveHeader header = {};

    Libc::Memory::kmemcpy((void*)&header, (void*)bufferVirtual, sizeof(header));

    return header;
}