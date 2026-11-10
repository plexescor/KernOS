#include "Libc/Memory.hpp"
#include <cstdint>
#include <cstddef>

#define HEAP_MIN_SPLIT 32ULL

static Libc::Memory::BlockHeader* headBlock = nullptr;

void Libc::Memory::kmemset(void* address, uint8_t value, size_t size)
{
    uint8_t* bytePtr = (uint8_t*)address;
    for (size_t i = 0; i < size; i++)
    {
        bytePtr[i] = value;
    }
}

void Libc::Memory::kmallocInit(uint64_t heapStart, uint64_t heapSize)
{
    headBlock = (Libc::Memory::BlockHeader*)heapStart;
    headBlock->size = heapSize - sizeof(Libc::Memory::BlockHeader);
    headBlock->isFree = true;
    headBlock->next = nullptr;
    headBlock->previous = nullptr;
}

void* Libc::Memory::kmalloc(uint64_t size)
{
    size = (size + 7) & ~7ULL;
    Libc::Memory::BlockHeader* currentBlock = headBlock;
    while (currentBlock)
    {
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
