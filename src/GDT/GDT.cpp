#include "GDT/GDT.hpp"

struct [[gnu::packed]] GDTEntry 
{
    uint16_t limitLow;   // limit bits 0-15
    uint16_t baseLow;    // base bits 0-15
    uint8_t  baseMid;    // base bits 16-23
    uint8_t  access;      // access byte
    uint8_t  flagsLimit; // upper 4 bits = flags, lower 4 = limit bits 16-19
    uint8_t  baseHigh;   // base bits 24-31
};

struct [[gnu::packed]] GDTDescriptor 
{
    uint16_t size;    // size of GDT - 1
    uint64_t offset;  // address of GDT
};

// 6 entries
static GDTEntry gdt[6];
static GDTDescriptor gdtr;

static void setEntry(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) 
{
    gdt[i].limitLow   = limit & 0xFFFF;
    gdt[i].baseLow    = base & 0xFFFF;
    gdt[i].baseMid    = (base >> 16) & 0xFF;
    gdt[i].access      = access;
    gdt[i].flagsLimit = ((flags & 0xF) << 4) | ((limit >> 16) & 0xF);
    gdt[i].baseHigh   = (base >> 24) & 0xFF;
}

extern "C" void gdtLoad(GDTDescriptor* gdtr);

void GDT::init() 
{
    // null
    setEntry(0, 0, 0,       0x00, 0x0);
    // kernel code (ring 0, 64-bit)
    setEntry(1, 0, 0xFFFFF, 0x9A, 0xA);
    // kernel data (ring 0)
    setEntry(2, 0, 0xFFFFF, 0x92, 0xC);
    // user data   (ring 3)
    setEntry(3, 0, 0xFFFFF, 0xF2, 0xC);
    // user code   (ring 3, 64-bit)
    setEntry(4, 0, 0xFFFFF, 0xFA, 0xA);
    // null placeholder for TSS (fill later)
    setEntry(5, 0, 0,       0x00, 0x0);

    gdtr.size   = sizeof(gdt) - 1;
    gdtr.offset = (uint64_t)&gdt;

    gdtLoad(&gdtr);
}
