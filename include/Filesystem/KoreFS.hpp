#pragma once

#include <cstdint>
#include <optional>

namespace Filesystem
{
    namespace KoreFS
    {
        // This is a Block header that resides in LBA 1 (512 or 4096 Bytes sector size, plenty in both cases)
        // It has a magic byte ("KOREFS ") to identify itself
        // It stores the size of each sector
        // It stores the total size of the drive (or partition? but entire drive for now) in sectors
        // It stores the used space in number of sectors
        // It stores the free space in number of sectors
        struct __attribute__((packed)) DriveHeader
        {
            char magic[8];
            uint64_t sectorSize;
            uint64_t driveSize;
            uint64_t usedSpace;
            uint64_t freespace;
        };
        // constexpr char MAGIC_BYTE[8] = {'K', 'O', 'R', 'E', 'F', 'S', ' ', ' '};
        bool driveHeaderExists();
        bool initialise();
        bool writeDriveHeader();
        std::optional<DriveHeader> readDriveheader();
    }
}