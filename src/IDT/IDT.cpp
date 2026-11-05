#include "IDT/IDT.hpp"

struct [[gnu::packed]] IDTEntry {
    uint16_t offsetLow;    // handler address bits 0-15
    uint16_t selector;     // kernel code selector (0x08)
    uint8_t  ist;      // just 0
    uint8_t  flags;    // type and attributes
    uint16_t offsetMid;    // handler address bits 16-31
    uint32_t offsetHigh;   // handler address bits 32-63
    uint32_t reserved;     // always 0
};

struct [[gnu::packed]] IDTDescriptor 
{
    uint16_t size;
    uint64_t offset;
};

static IDTEntry idt[256];
static IDTDescriptor idtr;

static void setEntry(int i, uint64_t handler, uint8_t flags) 
{
    idt[i].offsetLow  = handler & 0xFFFF;
    idt[i].selector   = 0x08;
    idt[i].ist    = 0;
    idt[i].flags      = flags;
    idt[i].offsetMid  = (handler >> 16) & 0xFFFF;
    idt[i].offsetHigh = (handler >> 32) & 0xFFFFFFFF;
    idt[i].reserved   = 0;
}

extern "C" 
{
    void isrDivideByZero();
    void isrDebug();
    void isrNMI();
    void isrBreakpoint();
    void isrOverflow();
    void isrBoundRange();
    void isrInvalidOpcode();
    void isrDeviceNotAvailable();
    void isrDoubleFault();
    void isrGPF();
    void isrPageFault();
    void isrTimer();
    void isrKeyboard();
}

extern "C" void idtLoad(IDTDescriptor* idtr);

extern "C" void isrHandler(uint64_t isrNum, uint64_t errorCode) {
    (void)isrNum;
    (void)errorCode;
    while (true) 
    {
        __asm__("hlt");
    }
}
    
void IDT::init() 
{
    setEntry(0, (uint64_t)isrDivideByZero,  0x8E);
    setEntry(1, (uint64_t)isrDebug, 0x8E);
    setEntry(2, (uint64_t)isrNMI, 0x8E);
    setEntry(3, (uint64_t)isrBreakpoint,     0x8E);
    setEntry(4, (uint64_t)isrOverflow, 0x8E);
    setEntry(5, (uint64_t)isrBoundRange, 0x8E);
    setEntry(6, (uint64_t)isrInvalidOpcode, 0x8E);
    setEntry(7, (uint64_t)isrDeviceNotAvailable, 0x8E);
    setEntry(8, (uint64_t)isrDoubleFault, 0x8E);
    setEntry(13, (uint64_t)isrGPF, 0x8E);
    setEntry(14, (uint64_t)isrPageFault, 0x8E);
    setEntry(32, (uint64_t)isrTimer, 0x8E);
    setEntry(33, (uint64_t)isrKeyboard, 0x8E);

    idtr.size   = sizeof(idt) - 1;
    idtr.offset = (uint64_t)&idt;

    idtLoad(&idtr);
}