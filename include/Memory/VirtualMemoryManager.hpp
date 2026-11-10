#pragma once
#include "Memory/PhysicalMemoryManager.hpp"
#include "limine.h"

#include <cstdint>
#include <cstddef>
#define PAGE_PRESENT        (1ULL << 0)  // Page is mapped in memory
#define PAGE_WRITABLE       (1ULL << 1)  // Page is read/write (0 = read-only)
#define PAGE_USER           (1ULL << 2)  // Accessible from user-space (Ring 3)
#define PAGE_CACHE_DISABLE  (1ULL << 4)  // Disable caching (needed for MMIO/framebuffer)
#define PAGE_NX             (1ULL << 63) // No-Execute (prevent running code on data)
#define VMM_ADDRESS_MASK 0x000FFFFFFFFFF000ULL // Extracts the 4KB-aligned physical address
struct PageTable
{
    uint64_t entries[512];
} __attribute__((aligned(4096)));

class VirtualMemoryManager
{
    public:
        VirtualMemoryManager(PhysicalMemoryManager* pmm, struct limine_memmap_response* memoryMap, struct limine_hhdm_response* response);
        ~VirtualMemoryManager() = default;
        void activate();
        void mapPage(uint64_t virtualAddress, uint64_t physicalAddress, uint64_t flags);
        void unMapPage(uint64_t virtualAddress);

    private:
        void mapKernelPrerequisites();
        uint64_t createTable();
        uint64_t physicalToVirtual(uint64_t address);
        uint64_t virtualToPhysical(uint64_t address);
        uint64_t getCR3();
        void setCR3(PageTable* pml4Address);
        PageTable* getNextTableLevel(PageTable* current, size_t index, uint64_t flags);
        PageTable* walkNextTableLevel(PageTable* current, size_t index);
        void flushTLB(uint64_t virtualAddress);

    private:
        PhysicalMemoryManager* pmm = nullptr;
        struct limine_memmap_response* memoryMap = nullptr;
        PageTable* pml4Table = nullptr;
        struct limine_hhdm_response* HHDMReponse = nullptr;
        uint64_t HHDMOffset = 0;
};
