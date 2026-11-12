#pragma once
#include <cstdint>
#include "limine.h"
class PhysicalMemoryManager
{
    public:
        PhysicalMemoryManager();
        ~PhysicalMemoryManager() = default;
        uint64_t allocFrame();
        void freeFrame_PMM(uint64_t address);
        struct limine_hhdm_response* getHDDM();
        struct limine_memmap_response* getMemoryMap_Limine();
    private:
        void findMaxAndUsableMemory();
        bool initialiseBitmap();

    public:
        uint64_t highestAddress;
        uint64_t totalUsable;

        uint8_t* bitmap;
        uint64_t totalPages;
        uint64_t bitmapBytes;
        uint64_t usedPages;
        uint64_t freePages;
};
