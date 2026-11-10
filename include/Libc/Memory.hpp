#pragma once
#include <cstdint>
#include <cstddef>

void* operator new(uint64_t size);
void operator delete(void* ptr, uint64_t size);
void* operator new[](uint64_t size);
void operator delete[](void* ptr, uint64_t size);
namespace Libc
{
    namespace Memory
    {
        struct BlockHeader
        {
            uint64_t size = 0;
            bool isFree = true;
            BlockHeader* next = nullptr;
            BlockHeader* previous = nullptr;
        };
        void kmemset(void* address, uint8_t value, size_t size);
        void kmallocInit(uint64_t heapStart, uint64_t heapSize);
        void* kmalloc(uint64_t size);
        void kfree(void* address);
    }
}
