#pragma once
#include <cstdint>
#include <cstddef>

void* operator new(unsigned long size);
void* operator new[](unsigned long size);
void operator delete(void* ptr);
void operator delete(void* ptr, unsigned long size);
void operator delete[](void* ptr);
void operator delete[](void* ptr, unsigned long size);
namespace Libc
{
    namespace Memory
    {
        struct BlockHeader
        {
            uint64_t size;
            bool isFree;
            BlockHeader* next;
            BlockHeader* previous;
        };
        void kmemset(void* address, uint8_t value, size_t size);
        void kmallocInit(uint64_t heapStart, uint64_t heapSize);
        void* kmalloc(uint64_t size);
        void kfree(void* address);
    }
}
