#pragma once

#include <cstdint>
#include <optional>

namespace Filesystem
{
    namespace KoreFS
    {
        struct __attribute__((packed)) FreeRegion
        {
            uint64_t startSector;
            uint64_t sectorCount;
        };
        // This is a Block header that resides in LBA 2048 (512 or 4096 Bytes sector size, plenty in both cases)
        // It has a magic byte ("KOREFS ") to identify itself
        // It stores the size of each sector
        // It stores the total size of the drive (or partition? but entire drive for now) in bytes, does not account for usableSectors
        // It stores the used space in number of bytes
        // It stores the free space in number of bytes
        // It stores the LBA of the FileMetadataHeader after which will come FolderDataHeader
        // It stores freeSectorCount
        struct __attribute__((packed)) DriveHeader
        {
            char magic[8];
            uint64_t sectorSize;
            uint64_t driveSize;
            uint64_t usedSpace;
            uint64_t freeSpace;
            uint64_t rootFolderHeaderSector;
            uint64_t freeRegionCount;
        };
        // constexpr char MAGIC_BYTE[8] = {'K', 'O', 'R', 'E', 'F', 'S', ' ', ' '};
        bool writeFreeRegion(FreeRegion* freeRegion, uint64_t driveHeaderStart);
        bool driveHeaderExists();
        bool initialise();
        bool writeDriveHeader();
        std::optional<DriveHeader> readDriveheader();
    }
}