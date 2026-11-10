#include "Memory/PhysicalMemoryManager.hpp"
#include "Libc/Memory.hpp"
#include "limine.h"
#include <cstdint>
#include <cstddef>

#define LIMINE_MEMMAP_REQUEST { LIMINE_COMMON_MAGIC, 0x67cf3d9d378a806f, 0xe304acdfc50c3c62 }
#define LIMINE_HHDM_REQUEST { LIMINE_COMMON_MAGIC, 0x48dcf1cb8ad2b852, 0x63984e959a98244b }

__attribute__((used, section(".limine_requests")))
volatile struct limine_memmap_request memoryMapRequest =
{
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
    .response = NULL
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_hhdm_request HHDMRequest =
{
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0,
    .response = NULL
};

#define PAGE_SIZE 4096

PhysicalMemoryManager::PhysicalMemoryManager()
{
    this->findMaxAndUsableMemory();
    bool _ = this->initialiseBitmap();
}

struct limine_memmap_response* PhysicalMemoryManager::getMemoryMap_Limine()
{
    if (memoryMapRequest.response == NULL
        || memoryMapRequest.response->entry_count < 1)
    {
        return nullptr;
    }
    return memoryMapRequest.response;
}

struct limine_hhdm_response* PhysicalMemoryManager::getHDDM()
{
    return HHDMRequest.response;
}

void PhysicalMemoryManager::findMaxAndUsableMemory()
{
    struct limine_memmap_response* memoryMap = this->getMemoryMap_Limine();
    if (!memoryMap) return;

    uint64_t highestAddress = 0;
    for (uint64_t i = 0; i < memoryMap->entry_count; i++)
    {
        struct limine_memmap_entry* entry = memoryMap->entries[i];

        uint64_t entryEnd = entry->base + entry->length;
        if (entryEnd > highestAddress)
        {
            highestAddress = entryEnd;
        }
        if (entry->type == LIMINE_MEMMAP_USABLE)
        {
            this->totalUsable += entry->length;
        }
    }

    // -1 because otherwise highestAddress would have the pointer to the byte after the lats byte
    this->highestAddress = highestAddress - 1;
}

bool PhysicalMemoryManager::initialiseBitmap()
{
    // Get hddm
    if (HHDMRequest.response == NULL)
    {
        return false;
    }
    // Calculate bitmap size and find space for it
    totalPages = highestAddress / PAGE_SIZE;
    bitmapBytes = (totalPages + 7) / 8;

    this->bitmap = nullptr;

    struct limine_memmap_response* memoryMap = this->getMemoryMap_Limine();
    if (!memoryMap) return false;

    // Find the first biggest memory region that can hold our bitmap
    for (uint64_t i = 0; i < memoryMap->entry_count; i++)
    {
        struct limine_memmap_entry* entry = memoryMap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE && bitmapBytes < entry->length)
        {
            bitmap = (uint8_t *)(entry->base + HHDMRequest.response->offset);
            break;
        }
    }

    // Set so that every page is used
    Libc::Memory::kmemset(bitmap, 0xFF, bitmapBytes);

    // Now make pages free if they are in usable regions
    for (uint64_t i = 0; i < memoryMap->entry_count; i++)
    {
        struct limine_memmap_entry* entry = memoryMap->entries[i];
        if (entry->type != LIMINE_MEMMAP_USABLE )
        {
            continue;
        }

        uint64_t base = entry->base / PAGE_SIZE;
        uint64_t count = entry->length / PAGE_SIZE;
        // While we are not at the page after the last page, keep clearing every bit
        for (uint64_t j = base; j < base + count; j++)
        {
            bitmap[j/8] &= ~(1 << (j % 8));
        }
    }
    // Now mark the pages used by our bitmap itself as used
    uint64_t bitmapPhysicalBase = (uint64_t)bitmap - HHDMRequest.response->offset;
    uint64_t bitmapPageStart = bitmapPhysicalBase / PAGE_SIZE;
    uint64_t bitmapPageSpan = (bitmapBytes + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint64_t j = bitmapPageStart; j < bitmapPageStart + bitmapPageSpan; j++)
    {
        bitmap[j / 8] |= (1 << (j % 8));
    }
    bitmap[0] |= (1 << 0);

    return true;
}

// Returns physical address of a page
uint64_t PhysicalMemoryManager::allocFrame_PMM()
{
    for (uint64_t byte = 0; byte < bitmapBytes; byte++)
    {
        // If all bits are occupied continue
        if (bitmap[byte] == 0xFF)
        {
            continue;
        }

        for (uint64_t bit = 0; bit < 8; bit++)
        {
            // If found 0 at a bit, mark it as used
            if (!(bitmap[byte] & (1 << bit)))
            {
                bitmap[byte] |= (1 << bit);
                uint64_t page = byte * 8 + bit;
                return page * PAGE_SIZE;
            }
        }
    }
    return 0; // Out
}

// Frees a page given its physical address
void PhysicalMemoryManager::freeFrame_PMM(uint64_t address)
{
    if (HHDMRequest.response == NULL)
    {
        return;
    }
    if (address % PAGE_SIZE != 0) return;
    // Get the physical address and Convert to page
    uint64_t page = address / PAGE_SIZE;
    uint64_t byte = page / 8;
    uint64_t bit = page % 8;

    bitmap[byte] &= ~(1 << bit);
}
