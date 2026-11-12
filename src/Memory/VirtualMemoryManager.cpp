#include "Memory/VirtualMemoryManager.hpp"
#include "Memory/PhysicalMemoryManager.hpp"
#include "Libc/Memory.hpp"
#include <cstdint>
#include "limine.h"
#include "Framebuffer/Framebuffer.hpp"

#define LIMINE_KERNEL_ADDRESS_REQUEST { LIMINE_COMMON_MAGIC, 0x71ba76863cc55f63, 0xb2644a48c516a487 }
__attribute__((used, section(".limine_requests")))
volatile struct limine_kernel_address_request  kernelAddressRequest =
{
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0,
    .response = NULL
};




VirtualMemoryManager::VirtualMemoryManager(PhysicalMemoryManager* pmm,
                                            struct limine_memmap_response* memoryMap,
                                            struct limine_hhdm_response* response)
    : pmm(pmm), memoryMap(memoryMap), HHDMReponse(response)
{
    if (!this->pmm) return;
    if (!this->memoryMap) return;
    if (!this->HHDMReponse) return;
    this->HHDMOffset = HHDMReponse->offset;
    // Allocate the 1st table
    this->pml4Table = (PageTable*)physicalToVirtual(createTable());
    if (!this->pml4Table) return;

    this->mapKernelPrerequisites();
    this->activate();
}

void VirtualMemoryManager::mapPage(uint64_t virtualAddress, uint64_t physicalAddress, uint64_t flags)
{
    size_t pml4Index = (virtualAddress >> 39) & 0x1FF;
    size_t pdptIndex = (virtualAddress >> 30) & 0x1FF;
    size_t pdIndex   = (virtualAddress >> 21) & 0x1FF;
    size_t ptIndex   = (virtualAddress >> 12) & 0x1FF;

    PageTable* pdpt = this->getNextTableLevel(pml4Table, pml4Index, flags);
    if (!pdpt) return;

    PageTable* pd = this->getNextTableLevel(pdpt, pdptIndex, flags);
    if (!pd) return;

    PageTable* pt = this->getNextTableLevel(pd, pdIndex, flags);
    if (!pt) return;

    pt->entries[ptIndex] = (physicalAddress & VMM_ADDRESS_MASK) | flags | PAGE_PRESENT;
    this->flushTLB(virtualAddress);
}

void VirtualMemoryManager::unMapPage(uint64_t virtualAddress)
{
    size_t pml4Index = (virtualAddress >> 39) & 0x1FF;
    size_t pdptIndex = (virtualAddress >> 30) & 0x1FF;
    size_t pdIndex   = (virtualAddress >> 21) & 0x1FF;
    size_t ptIndex   = (virtualAddress >> 12) & 0x1FF;

    PageTable* pdpt = this->walkNextTableLevel(pml4Table, pml4Index);
    if (!pdpt) return;

    PageTable* pd = this->walkNextTableLevel(pdpt, pdptIndex);
    if (!pd) return;

    PageTable* pt = this->walkNextTableLevel(pd, pdIndex);
    if (!pt) return;

    pt->entries[ptIndex] = 0;
    this->flushTLB(virtualAddress);
}

void VirtualMemoryManager::mapKernelPrerequisites()
{
    // Will tripple fault if this happens
    if (kernelAddressRequest.response == NULL) return;

    this->HHDMOffset = HHDMReponse->offset;
    // We get the physical and Virtual address of the kernel loaded by limine
    uint64_t physicalKernelBase = kernelAddressRequest.response->physical_base;
    uint64_t virtualKernelBase = kernelAddressRequest.response->virtual_base;
    uint64_t framebufferBase = (uint64_t)(Framebuffer::getFramebuffer(0)->address);
    uint64_t framebufferSize = Framebuffer::getFramebuffer(0)->height * Framebuffer::getFramebuffer(0)->pitch;

    // Map kernel and hhdm
    for (size_t i = 0; i < memoryMap->entry_count; i++)
    {
        struct limine_memmap_entry* entry = memoryMap->entries[i];

        if (entry->type == LIMINE_MEMMAP_RESERVED || entry->type == LIMINE_MEMMAP_BAD_MEMORY)
        {
            continue;
        }

        uint64_t physicalStart = entry->base & ~0xFFFULL;
        uint64_t physicalEnd   = (entry->base + entry->length + 0xFFFULL) & ~0xFFFULL;

        // Per page essentially
        for (uint64_t physical = physicalStart; physical < physicalEnd; physical += 4096)
        {
            this->mapPage(physicalToVirtual(physical), physical, PAGE_PRESENT | PAGE_WRITABLE | PAGE_NX);
            // Map kernel memory also
            if (entry->type == LIMINE_MEMMAP_KERNEL_AND_MODULES)
            {
                // Get the current virtual address
                uint64_t currentVirtual = (physical - physicalKernelBase) + virtualKernelBase;
                this->mapPage(currentVirtual, physical, PAGE_PRESENT | PAGE_WRITABLE);
            }
        }
    }

    // Map framebuffer
    uint64_t fbStart = framebufferBase & ~0xFFFULL;
    uint64_t fbEnd   = (framebufferBase + framebufferSize + 0xFFFULL) & ~0xFFFULL;

    for (uint64_t virtualAddress = fbStart; virtualAddress < fbEnd; virtualAddress += 4096)
    {
        uint64_t physicalAddress = this->virtualToPhysical(virtualAddress);
        // Fuck cache
        this->mapPage(virtualAddress, physicalAddress, PAGE_PRESENT | PAGE_WRITABLE | PAGE_NX | PAGE_CACHE_DISABLE);
    }
}

uint64_t VirtualMemoryManager::createTable()
{
    uint64_t physical = pmm->allocFrame();
    // TODO: Verify OOM
    Libc::Memory::kmemset((uint8_t*)physicalToVirtual(physical), 0, 4096);
    return physical;
}

void VirtualMemoryManager::activate()
{
    this->setCR3((PageTable*)virtualToPhysical((uint64_t)this->pml4Table));
}

uint64_t VirtualMemoryManager::physicalToVirtual(uint64_t address)
{
    return address + HHDMOffset;
}

uint64_t VirtualMemoryManager::virtualToPhysical(uint64_t address)
{
    return address - HHDMOffset;
}

uint64_t VirtualMemoryManager::getCR3()
{
    uint64_t value;
    __asm__ __volatile__ (
        "movq %%cr3, %0"
        : "=r" (value)
            :
            : "memory"
    );

    return value;
}

void VirtualMemoryManager::setCR3(PageTable* pml4Address)
{
    uint64_t value = (uint64_t)pml4Address;
    __asm__ __volatile__ (
        "movq %0, %%cr3"
        :
        : "r" (value)
        : "memory"
    );
}

PageTable* VirtualMemoryManager::getNextTableLevel(PageTable* current, size_t index, uint64_t flags)
{
    if (current->entries[index] & PAGE_PRESENT)
    {
        uint64_t nextPhysical = current->entries[index] & VMM_ADDRESS_MASK;
        return (PageTable*) this->physicalToVirtual(nextPhysical);
    }
    // Table doesnt exist, create it
    uint64_t nextPhysical = createTable();
    uint64_t midFlags = PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER);
    current->entries[index] = nextPhysical | midFlags;

    return (PageTable*)(this->physicalToVirtual(nextPhysical));
}

PageTable* VirtualMemoryManager::walkNextTableLevel(PageTable* current, size_t index)
{
    if (!(current->entries[index] & PAGE_PRESENT))
    {
        return nullptr;
    }

    uint64_t physicalAddress = current->entries[index] & VMM_ADDRESS_MASK;
    return (PageTable*)(this->physicalToVirtual(physicalAddress));
}

void VirtualMemoryManager::flushTLB(uint64_t virtualAddress)
{
    __asm__ volatile("invlpg (%0)" : : "r"(virtualAddress) : "memory");
}
