#pragma once
#include <cstdint>
#include <cstddef>
#include "Memory/PhysicalMemoryManager.hpp"
#include "Memory/VirtualMemoryManager.hpp"

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
        #define KERNEL_HEAP_START     0xFFFFB00000000000
        #define KERNEL_HEAP_INITIAL   (1024ULL * 1024ULL * 16ULL)
        #define KERNEL_HEAP_MAX       0xFFFFC00000000000
        struct BlockHeader
        {
            uint64_t size;
            bool isFree;
            BlockHeader* next;
            BlockHeader* previous;
        };
        void kmemset(void* address, uint8_t value, size_t size);
        bool kgrowHeap(uint64_t size, Libc::Memory::BlockHeader* lastBlock);
        bool kmallocInit(uint64_t heapStart, uint64_t heapSize, VirtualMemoryManager* vmm_P, PhysicalMemoryManager* pmm_P);
        void* kmalloc(uint64_t size);
        void kfree(void* address);
    }
}
