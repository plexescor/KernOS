# KoreFS

This document explains what is KoreFS and how does it work....
It is an absolute abomination of a Filesystem.

KoreFS is a custom filesystem specifically designed for KernOS.

> [!NOTE]
> It doesn't comply with POSIX, isn't compatible with other filesystems..Though I also want to mention that this spec/documentation is mostly complete, KernOS hasn't fully implemented it yet, work is still going.

## Some Points:

- Sector Based design, theres no block groups or `inodes` or shit. There are only things called `DriveHeader` or `FileMetadataHeader` or `FolderDataHeader`.
- Magic identifier: `KOREFS  `.
- Designed to be simple to implement but still not be shit.
- Folders are technically a file (Though I dont believe in ***Everything is a file*** philosphy)

## Disk Layout:

- LBA 0 to LBA 2047 are untouched, they are reserved for GPT headers, EFI partitions etc.
- `DriveHeader` resides at LBA 2048, exactly 1 sector wide, this is the *heart* of KoreFS.
- Starting from LBA 2049 and spanning to however many sectors needed, a flexible X-GiB `FreeRegion` is reserved, whose only purpose is to track free space.
- Immediately after the `FreeRegion`'s sector, comes the **Root** folder's `FileMetaDataHeader`, then the `FolderDataHeader`.
- Everything after that are the User data (including but not limited to system files).
- Reserved sectors formula: (reservedRegionSize)/sectorSize.
- rootFolderSector = 2049 + reservedSectors.

> [!NOTE]
> In future, the EFI partition will need to be accomodated, hence the `2048` number is not concrete and may shift.

## Size of Reserved Region Itself and 'Worst Case':

- The amount of space which needs to be reserved for the **Reserved Region** itself is calculated using driveSize and sector size
- Lets take an example:
  - 512GiB Drive with Sector Size = 4096
- We will now need totalSectors:
  - totalSectors = Disk Size in bytes / sector size, SO: 549755813888 / 4096 = 134217728 sectors
- Now before we move further, lets discuss whats `Worst Case`
  - Assume the drive gets soo fragmented that essentially free sectors are in this order:
  ```C++
  [USED][FREE][USED][FREE][USED][FREE] and so on....
  ```
  - Now in this case, the number of entries of FreeRegion in the Reserved Region would be the maximum(Well not quite, just add +1 to the number of entries)
  - Now maxEntries = totalSectors / 2 + 1, SO: 134217728/2 + 1 = 67108865
  - So we need the `Reserved Region` to be big enough to hold as many entries as the worst case..
- So now finally the bytes needed for the `Reserved Region` itself would be: 
  ```C++
  (maxEntries * sizeof(FreeRegion)) + (32 * 1024 * 1024);
  ```
  - Where sizeof(FreeRegion) == 16 bytes
  - And your eye might have caught that we are reserving extra 32 MiB, well its just an `emergency` buffer but anyways.

> [!NOTE]
> If you weren't able to understand this or i explained shit, just open a github issue or email me: `saraswal2010@gmail.com`.

## DriveHeader:

- It lives at LBA 2048 and is exactly 1 sector wide (be it 512 or 4096 bytes), its read once at startup
- Fields:
  ```C++
  char magic[8]; // "KOREFS  "
  uint64_t sectorSize; // bytes 
  uint64_t driveSize; // bytes
  uint64_t usedSpace; // bytes
  uint64_t freeSpace; // bytes
  uint64_t rootFolderHeaderSector; // LBA where "FileMetadataHeader" resides
  uint64_t freeRegionCount; // Number of free regions available, set to 1 during formatting

  uint64_t maxEntries; // Max entries possible in a worst case in the free region reserved region
  uint64_t reservedRegionsBytes; // The size of that reserved region in bytes
  uint64_t reservedRegionSpan; // Its sector span
  ```
- Its the single source of truth
- If it gets corrupted, shit's cooked. I plan to add some sort of backup copies in future.

## FreeRegion system

- Well its similar to `extents`
- A FreeRegion is a struct of two uint64_t fields: startSector, and sectorSpan
- The FreeRegion table occupies reserved X-GiB space starting for LBA 2049, Just divide the size in bytes by sizeof(FreeRegion) struct and you get the max entries it can store which are equal to worst case scenario.
- Can get fragmented but I will try to take some measures, I haven't done detailed maths though.
- **Allocation**: Scan the table for the first span with sectorSpan >= sectorsNeeded, take some sectors from its front, shrink it, if it hits zero sectors, remove it from the table and decrement `freeRegionCount` in the DriveHeader
- **Deallocation**: If a file gets deleted, its span gets returned, if something adjacent is also free in either direction, merge those shit, otherwise create a new entry and increment `freeRegionCount` in the DriveHeader
- **Fragmented Allocation**: The demon... Try to allocate in contiguous N sectors, if we cant find them, we need to fragment it... Calculate how many sectors (chunks) it will need, excluding the first sector where it will be stored (Because in contiguous files in KernOS, chunks = 0, I hope i made myself clear, though I doubt that I explained in a good manner) and write those into the `FileMetadataHeader`, also write the starting LBA of a chunk and the LBA span of that chunk into the array (more on this in the next section, because you may be confused) MAX_CHUNKS = 128 (hardcoded, shit is cooked when it cant find even 2 sectors which are contiguous).
- The FreeRegion table itself is stored on the X-GiB reserved block as said earlier, and updated on every allocation and deallocation (I havent decided if this will be sync or async, its still the early days)
- If the FreeRegion table tells that startSector == 0 && sectorSpan == 0, then that region is considered empty. 

## FileMetadataHeader

- Every file and folder on disk starts with `FileMetadataHeader`.
- Fields:
  ```C++
  char magic[8]; // "KOREFILE"
  char name[128];
  uint64_t sizeBytes; // We can just sum the children's sizes?
  uint64_t sectorSpan; //rounded, not sure about folder
  uint8_t type; // 0 = file, 1 = folder
  uint64_t flags; // Not used currently
  uint64_t chunkCount; 
  // chunkCount was what i was talking about, if this is 0, then  this file fits in 1 sector (usually the same sector as the header), MAX_CHUNKS=128 btw
  uint64_t chunkSectorStart[MAX_CHUNKS]; // Contains LBAs, position matters
  uint64_t chunkSectorSpan[MAX_CHUNKS]; // Contains their Spans, position matters
  uint64_t dataOffset; // We will come back to this
  ```
- Some issues which are shit (We will call this issue: ***2217 Horror***, remember that)
  - Now the issue, now its needed that the size of the struct is inferrebale at compile time, if it were to be donem the size would have been 2217 bytes. 
  - As you can imagine, if we write this header to disk for like say a 100 byte file, the file would fit in 1 sector but the header will take ~5 sectors (on 512 sectorSize drive) and this is wasteful, it adds up fast.
- The solution to the above issue
  - We use that struct to just access shit in memory, i dont know whats the right framing for this but forgive me, lets just move to next point...
  - We will only write the entries upto chunkCount to the disk from the in-Memory Header to the `FileMetadataHeader` which is on the disk. We know have 169 (nice number) worth of data on the disk, and then as chunk count increases, we write extra 16 bytes worth of data with each increment in chunkCount (for chunkCount == 0, we write 0 * 16 bytes).. So this way, we only write the chunkSectorStart and chunkSectorSpan entries which do actually exist..
- The projected outcome of the above solution:
  - If we keep the header ~2217 bytes, for one million files, thats extra ~1.8GB. 
  - Basically I am solving a problem which i created...
- SectorSpan formula = ceil((169 + chunkCount * 16 + sizeBytes) / sectorSize) Note that the sectorSpan on the disk and the struct only represents the Current chunk's (or current file if its contiguous) sector span.
- Data offset is computed during deserialization as 169 + (chunkCount * 16), it points to exactly where the file data begins in the sector buffer, after reading the data, we can jump to the next LBA.


## Serialize and deserialize (I dont like this section so i rushed while typing it)

- We cant just do a blind kmemcpy because of ***2217 Horror***
- Though we can do byte by byte careful kmemcpy
- We will need to serialize (manually copy byte by byte) so basically write speeds are fucked.
- We will read manually byte by byte into a buffer and deserialize, and also assign the dataOffset by reading from the disk.
- If there are no chunks (chunkCount == 0) then we just read the data at the offset, no need to deserialize anything.
- These two functions are backbone for reading and writing, if you didnt guess even with plenty of time.

## Folders

- Folders are just files which happen to be special (type == 1 in the `FileMetadataHeader`) and they have children btw.
- When you go to the `dataOffset` of a folder header, you come at `FolderDataHeader` which contains:
  ```C++
  uint64_t childCount; // Read the name of the variable
  uint64_t childHeaderSector[]; // Contains many LBAs to "FileMetadataHeader"s
  ```
- The field `childHeaderSector` contains LBAs to `FileMetadataHeader`s and they are sorted by name lexicographically (Because binary search optimisation)
- As said earlier, Path traversals use Binary Search to find by specific paths/files/folders by name.
  - Reads are ~O(log n).
  - Writes are well ~O(n^2) (My time complexity analysis may be wrong, dont treat this as concrete truth).
- Root Folder's `FileMetadataHeader` lives at the rootFolderHeaderSector field inside the `DriveHeader`.

> [!NOTE]
> FolderDataHeader suffers from the same ***2217 Horror*** as `FileMetadataHeader`. In memory we would need a fixed max children array, but on disk only childCount entries are written. Same serialize/deserialize approach applies, read childCount first, then read exactly that many uint64_t LBAs after it. 

## File allocation walkthrough

- KoreFS will always try to allocate a file in contiguous manner.. It finds a FreeRegion with enough sectors, writes the `FileMetadataHeader` + data into the sectors, shrinks the FreeRegion, for a 100 byte file ceil((169 + 100) / 512) = 1 sector... So the header and file sits in a single sector... One thing if we try to put multiple small files into a single sector, would shit breakdown? I guess yes though I will try to test this
- If no single FreeRegion is big enough, KoreFS will write the file into multiple sectors even if it causes fragmentation, each chunk gets recorded and written into the `FileMetadataHeader`... And if the fragmentation is insane, you wont be able to write any file at all (if MAX_CHUNKS are exceeded that is)... This can happen on a drive that is almost completely full and has been written and deleted randomly thousands of time. On a fresh or normally used drive, this wont happen... But if it does happen, we can always increase MAX_CHUNKS, you would then be able to store files but well the softwarre overhead of issueing multiple NVMe commands and reassmebling chunks in memory will be well, questionable. The CPU will disagree...
- Always use ceil so nothing is trucated.

## Now the limiations and issues and Future work

- No crash safety, the filesystem can be corrupted during a powerloss while writing. COW will help mitigate this but its currently overkill in this early stage.
- No permissions, though uint64_t flags exist, but we can always modify the struct however as much as we want before v1.0.
- No timestamps.
- No GPT compliance.
- No EFI compliance.
- No concept of multiple NVMe or multiple Partitions.
- No limit on size of children of a directory, though i dont think this is an issue.