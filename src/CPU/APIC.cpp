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
#define LAPIC_BASE 0xFEE00000
#define SPURIOUS_VECTOR_REG 0xF0
#define LAPIC_SOFTWARE_ENABLE (1 << 8)
#define LAPIC_EOI_REG 0xB0

// IOAPIC
#define IOAPIC_BASE 0xFEC00000
#define IOREGSEL IOAPIC_BASE + 0x00
#define IOREGWIN IOAPIC_BASE + 0x10
#define IOAPICID 0x00
#define IOAPICVER 0x01
#define IOAPICARB 0x02
#define IOAPICREDTBL(n) (0x10 + 2 * n)


static VirtualMemoryManager* vmm = nullptr;

void CPU::APIC::setVMM(VirtualMemoryManager* v)
{
    vmm = v;
}

void CPU::APIC::mapMMIO(uint64_t physBase, uint64_t size)
{
    uint64_t pages = (size + 4095) / 4096;
    for (uint64_t i = 0; i < pages; i++)
    {
        uint64_t addr = physBase + i * 4096;
        vmm->mapPage(addr, addr, PAGE_PRESENT | PAGE_WRITABLE | PAGE_CACHE_DISABLE);
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

bool CPU::APIC::initialiseLAPIC()
{
    CPU::APIC::disableLegacyPIC();
    if (!CPU::APIC::checkLAPICSupport()) return false;
    CPU::APIC::enableLAPICGlobally();

    mapMMIO(LAPIC_BASE, 4096);
    mapMMIO(IOAPIC_BASE, 4096);

    volatile uint32_t* spuriousReg = (volatile uint32_t*)(LAPIC_BASE + SPURIOUS_VECTOR_REG);

    *spuriousReg = 0xFF | LAPIC_SOFTWARE_ENABLE;
    return true;
}

void CPU::APIC::LAPICEOI()
{
    volatile uint32_t* EOIRegister = (volatile uint32_t*)(LAPIC_BASE + LAPIC_EOI_REG);
    *EOIRegister = 0;
}

// Offset is where we wanna read from ig
uint32_t CPU::APIC::readIOAPIC(uint8_t offset)
{
    // Tell IOREGSEL where we wanna read from
    *(volatile uint32_t*)(IOREGSEL) = offset;
    // Read from the output or IOREGWIN register ig
    return *(volatile uint32_t*)(IOREGWIN);
}

void CPU::APIC::writeIOAPIC(uint8_t offset, uint32_t value)
{
    // Tell IOREGSEL where we wanna write to
    *(volatile uint32_t*)(IOREGSEL) = offset;
    // Write the value into IOREGWIN which is to be written
    *(volatile uint32_t*)(IOREGWIN) = value;
}

bool CPU::APIC::initialiseIOAPIC()
{

}

void CPU::APIC::IOAPICSetupKeyboard(uint8_t IDTVector, uint8_t LAPICID)
{
    uint32_t lowBits = 0x00000000 | IDTVector;
    uint32_t highBits = ((uint32_t)LAPICID) << 24;

    // Keyboard is Pin-1
    writeIOAPIC(IOAPICREDTBL(1), lowBits);
    writeIOAPIC(IOAPICREDTBL(1) + 1, highBits);

    IO::Port::inb(0x61);

    // Drain any pending scancodes from the PS/2 buffer
    while (IO::Port::inb(0x64) & 0x1)
        IO::Port::inb(0x60);

    // Clear any stuck LAPIC interrupt
    CPU::APIC::LAPICEOI();
}
