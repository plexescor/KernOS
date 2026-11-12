#include "CPU/APIC.hpp"
#include "CPU/MSR.hpp"
#include "IO/PortIO.hpp"

#include <cstdint>
#include <cpuid.h>

// TODO: Mask the interrupts durning boot process
// PIC related macros
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

// LAPIC
#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_BSP 0x100 // Processor is a BSP
#define IA32_APIC_BASE_MSR_ENABLE 0x800
#define SPURIOUS_VECTOR_REG 0xF0
#define LAPIC_SOFTWARE_ENABLE (1 << 8)
#define LAPIC_EOI_REG 0xB0

// IOAPIC
#define IOAPICID 0x00
#define IOAPICVER 0x01
#define IOAPICARB 0x02
#define IOAPICREDTBL(n) (0x10 + 2 * n)


extern VirtualMemoryManager* vmm_G;

static uint64_t IOAPICBase = 0xFEC00000;
static uint32_t keyboardGSI = 1; // Safe defaults
static uint16_t keyboardFlags = 0;
static uint64_t LAPICBase = 0xFEE00000;  // safe default

void CPU::APIC::mapMMIO(uint64_t physBase, uint64_t size)
{
    uint64_t pages = (size + 4095) / 4096;
    for (uint64_t i = 0; i < pages; i++)
    {
        uint64_t addr = physBase + i * 4096;
        vmm_G->mapPage(addr, addr, PAGE_PRESENT | PAGE_WRITABLE | PAGE_CACHE_DISABLE);
    }
}

void CPU::APIC::disableLegacyPIC()
{
    IO::Port::outb(PIC1_DATA, 0xFF);
    IO::Port::outb(PIC2_DATA, 0xFF);
}

void CPU::APIC::enableLAPICGlobally()
{
    uint32_t eax = 0, edx = 0;
    CPU::MSR::getMSR(IA32_APIC_BASE_MSR, &eax, &edx);
    eax = eax | IA32_APIC_BASE_MSR_ENABLE;
    CPU::MSR::writeMSR(IA32_APIC_BASE_MSR, eax);
}

bool CPU::APIC::checkLAPICSupport()
{
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    __get_cpuid(1, &eax, &ebx, &ecx, &edx);
    return (edx & (1 << 9)) != 0;
}

bool CPU::APIC::initialiseLAPIC(uint64_t LAPICBase_)
{
    LAPICBase = LAPICBase_;
    CPU::APIC::disableLegacyPIC();
    if (!CPU::APIC::checkLAPICSupport()) return false;
    CPU::APIC::enableLAPICGlobally();

    mapMMIO(LAPICBase, 4096);

    volatile uint32_t* spuriousReg = (volatile uint32_t*)(LAPICBase + SPURIOUS_VECTOR_REG);

    *spuriousReg = 0xFF | LAPIC_SOFTWARE_ENABLE;
    return true;
}

void CPU::APIC::LAPICEOI()
{
    volatile uint32_t* EOIRegister = (volatile uint32_t*)(LAPICBase + LAPIC_EOI_REG);
    *EOIRegister = 0;
}

// Offset is where we wanna read from ig
uint32_t CPU::APIC::readIOAPIC(uint8_t offset)
{
    // Tell IOREGSEL where we wanna read from
    *(volatile uint32_t*)(IOAPICBase + 0x00) = offset;
    // Read from the output or IOREGWIN register ig
    return *(volatile uint32_t*)(IOAPICBase + 0x10);
}

void CPU::APIC::writeIOAPIC(uint8_t offset, uint32_t value)
{
    // Tell IOREGSEL where we wanna write to
    *(volatile uint32_t*)(IOAPICBase + 0x00) = offset;
    // Write the value into IOREGWIN which is to be written
    *(volatile uint32_t*)(IOAPICBase + 0x10) = value;
}
#include "TTY/TTY.hpp"

bool CPU::APIC::initialiseIOAPIC(uint64_t address, uint32_t keyboardGSI_P, uint16_t keyboardFlags_P)
{
    TTY::putString("[INIT] ", TTY::Color::CYAN);
    TTY::putString("Initializing IOAPIC...\n", TTY::Color::WHITE);
    // TTY::putString("initialiseIOAPIC called!\n", TTY::Color::LIME);
    IOAPICBase = address;
    keyboardGSI = keyboardGSI_P;
    keyboardFlags = keyboardFlags_P;
    mapMMIO(IOAPICBase, 4096);
    return true;
}

static void ps2Wait()
{
    uint32_t timeout = 100000;
    while ((IO::Port::inb(0x64) & 0x2) && timeout--);
}

static void ps2WaitRead()
{
    uint32_t timeout = 100000;
    while (!(IO::Port::inb(0x64) & 0x1) && timeout--)
        ;
}
// uint8_t CPU::APIC::getLAPICID()
// {
//     volatile uint32_t* lapicID = (volatile uint32_t*)(vmm->physicalToVirtual(LAPIC_BASE) + 0x20);
//     return (*lapicID >> 24) & 0xFF;
// }
void CPU::APIC::initPS2()
{
    ps2Wait(); IO::Port::outb(0x64, 0xAD);
    ps2Wait(); IO::Port::outb(0x64, 0xA7);

    IO::Port::inb(0x60);

    ps2Wait(); IO::Port::outb(0x64, 0x20);
    ps2WaitRead();
    uint8_t config = IO::Port::inb(0x60);

    config |= (1 << 0);
    config &= ~(1 << 1);

    ps2Wait(); IO::Port::outb(0x64, 0x60);
    ps2Wait(); IO::Port::outb(0x60, config);

    ps2Wait(); IO::Port::outb(0x64, 0xAE);

    ps2Wait(); IO::Port::outb(0x60, 0xFF);
    ps2WaitRead(); IO::Port::inb(0x60);
    ps2WaitRead(); IO::Port::inb(0x60);

    ps2Wait(); IO::Port::outb(0x60, 0xF4);
    ps2WaitRead(); IO::Port::inb(0x60);
}

void CPU::APIC::IOAPICSetupKeyboard(uint8_t IDTVector, uint8_t LAPICID)
{
    uint8_t status = IO::Port::inb(0x64);
    if (status == 0xFF)
    {
        // 0xFF means no controller present
        TTY::putString("[WARN] No PS/2 controller found!\n", TTY::Color::YELLOW);
    }
    CPU::APIC::initPS2();
    uint32_t lowBits = IDTVector;

    if (keyboardFlags & 0x2)
        lowBits |= (1 << 13);

    if (keyboardFlags & 0x8)
        lowBits |= (1 << 15);
    uint32_t highBits = ((uint32_t)LAPICID) << 24;

    writeIOAPIC(IOAPICREDTBL(keyboardGSI), lowBits);
    writeIOAPIC(IOAPICREDTBL(keyboardGSI) + 1, highBits);

    IO::Port::inb(0x61);

    // Drain any pending scancodes from the PS/2 buffer
    while (IO::Port::inb(0x64) & 0x1)
        IO::Port::inb(0x60);

    // Clear any stuck LAPIC interrupt
    CPU::APIC::LAPICEOI();
}
