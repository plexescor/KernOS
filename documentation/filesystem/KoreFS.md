# KoreFS

This document explains what is KoreFS and how does it work....
It is an absolute abomination of a Filesystem.

KoreFS is a custom filesystem specifically designed for KernOS.

> [!NOTE]
> It doesn't comply with POSIX, isn't compatible with other filesystems..

## Some Points:

- Sector Based design, theres no block groups or `inodes` or shit. There are only things called `DriveHeader` or `FileMetadataHeader` or `FolderDataHeader`.
- Magic identifier: `KOREFS  `.
- Designed to be simple to implement but still not be shit.
- Folders are technically a file (Though I dont believe in ***Everything is a file*** philosphy)

## Disk Layout:

- LBA 0 to LBA 2047 are untouched, they are reserved for GPT headers, EFI partitions etc.
- `DriveHeader` resides at LBA 2048, exactly 1 sector wide, this is the *heart* of KoreFS.
- Starting from LBA 2049 and spanning to however many sectors needed, a 1 GB `FreeRegion` is reserved, whose only purpose is to track free space.
- Immediately after the `FreeRegion`'s sector, comes the **Root** folder's `FileMetaDataHeader`, then the `FolderDataHeader`.
- Everything after that are the User data (including but not limited to system files).
- Reserved sectors formula: (1GB)/sectorSize.
- rootFolderSector = 2049 + reservedSectors.

> [!NOTE]
> In future, the EFI partition will need to be accomodated, hence the `2048` number is not concrete and may shift.

## DriveHeader:

- It lives at LBA 2048 and is exactly 1 sector wide (be it 512 or 4096 bytes), its read once at startup
- Fields:
  ```
  char magic[8]; // "KOREFS  "
  uint64_t sectorSize; // bytes 
  uint64_t driveSize; // bytes
  uint64_t usedSpace; // bytes
  uint64_t freeSpace; // bytes
  uint64_t rootFolderHeaderSector; // LBA where "FileMetadataHeader" resides
  uint64_t freeRegionCount; // Number of free regions available
  ```