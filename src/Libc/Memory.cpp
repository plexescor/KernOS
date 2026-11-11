#include "Libc/Memory.hpp"
#include "Memory/PhysicalMemoryManager.hpp"
#include "Memory/VirtualMemoryManager.hpp"

#include <cstdint>
#include <cstddef>

#define HEAP_MIN_SPLIT 32ULL

void* operator new(uint64_t size)
{
    return Libc::Memory::kmalloc(size);
}

void operator delete(void* ptr)
{
    Libc::Memory::kfree(ptr);
}
void* operator new[](uint64_t size)
{
    return Libc::Memory::kmalloc(size);
}

void operator delete[](void* ptr)
{
    Libc::Memory::kfree(ptr);
}
void operator delete(void* ptr, uint64_t size)
{
    (void)size;
    Libc::Memory::kfree(ptr);
}

void operator delete[](void* ptr, uint64_t size)
{
    (void)size;
    Libc::Memory::kfree(ptr);
}

static Libc::Memory::BlockHeader* headBlock = nullptr;
static VirtualMemoryManager* vmm = nullptr;
static PhysicalMemoryManager* pmm = nullptr;

void Libc::Memory::kmemset(void* address, uint8_t value, size_t size)
{
    uint8_t* bytePtr = (uint8_t*)address;
    for (size_t i = 0; i < size; i++)
    {
        bytePtr[i] = value;
    }
}

// BUG the memory calculation is slightly off, sometimes it will overallocate by ~32 bytes (estimate)
// If the lastBlock is free but doesnt fit the requested size
bool Libc::Memory::kgrowHeap(uint64_t size, Libc::Memory::BlockHeader* lastBlock)
{
    // Assume pmm and vmm are not nullptr, as kheapInit() handles that
    // Get the address at the end of the data part of the last block
    uint64_t lastAddress = (uint64_t)(lastBlock + 1) + lastBlock->size;
    // Get how many pages we need
    uint64_t pages = (uint64_t)((sizeof(Libc::Memory::BlockHeader) + size + 4095) / 4096);

    // Now map the required pages
    for (uint64_t page = 0; page < pages; page++)
    {
        uint64_t physicalAddress = pmm->allocFrame_PMM();
        if (!physicalAddress) return false;
        vmm->mapPage(lastAddress + page * 4096, physicalAddress, PAGE_PRESENT | PAGE_WRITABLE | PAGE_NX);
    }

    // Now if the last block is free, then just extend it
    if (lastBlock->isFree)
    {
        lastBlock->size += pages * 4096;
        return true;
    }
    // If last block isnt free, create a new block
    Libc::Memory::BlockHeader* newBlock = (Libc::Memory::BlockHeader*)lastAddress;
    newBlock->size = (pages * 4096 - sizeof(Libc::Memory::BlockHeader));
    newBlock->isFree = true;
    newBlock->next = nullptr;
    newBlock->previous = lastBlock;
    lastBlock->next = newBlock;
    return true;
}

// P stands for parameter variant
bool Libc::Memory::kmallocInit(uint64_t heapStart, uint64_t heapSize, VirtualMemoryManager* vmm_P, PhysicalMemoryManager* pmm_P)
{
    if (!vmm_P || !pmm_P) return false;
    vmm = vmm_P;
    pmm = pmm_P;
    headBlock = (Libc::Memory::BlockHeader*)heapStart;
    headBlock->size = heapSize - sizeof(Libc::Memory::BlockHeader);
    headBlock->isFree = true;
    headBlock->next = nullptr;
    headBlock->previous = nullptr;
    return true;
}

void* Libc::Memory::kmalloc(uint64_t size)
{
    size = (size + 7) & ~7ULL;
    Libc::Memory::BlockHeader* currentBlock = headBlock;
    Libc::Memory::BlockHeader* lastBlock = currentBlock;
    retry:
        while (currentBlock)
        {
            lastBlock = currentBlock;
            if (!currentBlock->isFree || currentBlock->size < size)
            {
                currentBlock = currentBlock->next;
                continue;
            }


            // We found a valid region of free memory
            // Check if its worth splitting
            if (currentBlock->size >= size + sizeof(Libc::Memory::BlockHeader) + HEAP_MIN_SPLIT)
            {
                // Go to the new block's header's starting position
                Libc::Memory::BlockHeader* newBlock = (Libc::Memory::BlockHeader*)((uint8_t*)(currentBlock + 1) + size);
                newBlock->size = currentBlock->size - size - sizeof(Libc::Memory::BlockHeader);
                newBlock->isFree = true;
                newBlock->next = currentBlock->next;
                newBlock->previous = currentBlock;
                if (currentBlock->next) currentBlock->next->previous = newBlock;
                currentBlock->next = newBlock;
                currentBlock->size = size;
            }

            // Return a pointer to data start
            currentBlock->isFree = false;
            return (void*)(currentBlock + 1);
        }
    // We didnt find a free block, so expand the heap, then go to retry..
    if (Libc::Memory::kgrowHeap(size, lastBlock))
    {
        currentBlock = lastBlock;
        goto retry;
    }
    // OOM
    return nullptr;
}

void Libc::Memory::kfree(void* address)
{
    if (!address) return;
    // Get the address of the *HeaderBlock*
    Libc::Memory::BlockHeader* currentBlock = (Libc::Memory::BlockHeader*)address - 1;
    currentBlock->isFree = true;

    // Check if the next header is also free:
    Libc::Memory::BlockHeader* nextBlock = currentBlock->next;
    // Libc::Memory::BlockHeader* nextBlock = (Libc::Memory::BlockHeader*)((uint8_t*)(currentBlock + 1) + currentBlock->size);
    if (nextBlock && nextBlock->isFree)
    {
        // Extend size of our current block to accomodate the next block size also
        currentBlock->size += nextBlock->size + sizeof(Libc::Memory::BlockHeader);
        currentBlock->next = nextBlock->next;
        if (nextBlock->next)
            nextBlock->next->previous = currentBlock;
    }

    // Check now if the previous header is also free:
    Libc::Memory::BlockHeader* previousBlock = currentBlock->previous;
    if (previousBlock && previousBlock->isFree)
    {
        previousBlock->size += currentBlock->size + sizeof(Libc::Memory::BlockHeader);
        previousBlock->next = currentBlock->next;
        if (currentBlock->next)
            currentBlock->next->previous = previousBlock;
    }
}
