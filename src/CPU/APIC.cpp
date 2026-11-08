#include "CPU/APIC.hpp"
#include "CPU/MSR.hpp"
#include "IO/PortIO.hpp"

#include <cstdint>
#include <cpuid.h>

// From wiki
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_BSP 0x100 // Processor is a BSP
#define IA32_APIC_BASE_MSR_ENABLE 0x800

#define LAPIC_BASE 0xFEE00000
#define SPURIOUS_VECTOR_REG 0xF0
#define LAPIC_SOFTWARE_ENABLE (1 << 8)

#define LAPIC_EOI_REG        0xB0

void CPU::APIC::disableLegacyPIC()
{
    IO::Port::outb(PIC1_DATA, 0xFF);
    IO::Port::outb(PIC2_DATA, 0xFF);
}

void CPU::APIC::enableLAPICGlobally()
{
    uint32_t eax = 0, edx = 0;
    CPU::MSR::getMSR(IA32_APIC_BASE_MSR_BSP, &eax, &edx);

    eax = eax | IA32_APIC_BASE_MSR_ENABLE;

    CPU::MSR::writeMSR(IA32_APIC_BASE_MSR, eax);
}

bool CPU::APIC::checkAPICSupport()
{
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    __get_cpuid(1, &eax, &ebx, &ecx, &edx);
    return (edx & (1 << 9)) != 0;
}

bool CPU::APIC::initialiseAPIC()
{
    CPU::APIC::disableLegacyPIC();
    if (!CPU::APIC::checkAPICSupport()) return false;
    CPU::APIC::enableLAPICGlobally();

    // TODO: Add this to Page (vmem)

    volatile uint32_t* spuriousReg = (volatile uint32_t*)(LAPIC_BASE + SPURIOUS_VECTOR_REG);

    *spuriousReg = 0xFF | LAPIC_SOFTWARE_ENABLE;
    return true;
}

void CPU::APIC::LAPICEOI()
{
    volatile uint32_t* EOIRegister = (volatile uint32_t*)(LAPIC_BASE + LAPIC_EOI_REG);
    *EOIRegister = 0;
}
