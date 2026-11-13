#include "CPU/APIC.hpp"
#include "CPU/MSR.hpp"
#include "IO/PortIO.hpp"

#include <cstdint>
#include <cpuid.h>

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
extern void uint64ToHex(uint64_t value, char* buffer);
extern const char* uint64ToString(uint64_t value, char* bufferOut, int bufferSize);

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

void CPU::APIC::IOAPICSetupKeyboard(uint8_t IDTVector, uint8_t LAPICID)
{
    uint8_t status = IO::Port::inb(0x64);
    if (status == 0xFF)
    {
        TTY::putString("[WARN] No PS/2 controller found!\n", TTY::Color::YELLOW);
        return;
    }

    // Initialize the 8042 PS/2 controller
    auto waitInput = []()
    {
        while (IO::Port::inb(0x64) & 0x2)
            IO::Port::IOWait();
    };

    waitInput();
    IO::Port::outb(0x64, 0xAE);   // Enable keyboard port
    IO::Port::IOWait();

    waitInput();
    IO::Port::outb(0x64, 0x20);   // Read config byte
    IO::Port::IOWait();
    uint8_t config = IO::Port::inb(0x60);
    config |= (1 << 0);            // Enable IRQ1
    config &= ~(1 << 4);           // Un-disable keyboard

    waitInput();
    IO::Port::outb(0x64, 0x60);   // Write config byte
    IO::Port::IOWait();
    waitInput();
    IO::Port::outb(0x60, config);
    IO::Port::IOWait();

    // Build REDTBL entry
    uint32_t lowBits = IDTVector;
    if (keyboardFlags & 0x2) lowBits |= (1 << 13);  // polarity
    if (keyboardFlags & 0x8) lowBits |= (1 << 15);  // trigger mode
    uint32_t highBits = ((uint32_t)LAPICID) << 24;

    // Debug prints
    char buf1[21], buf2[21];
    char buf3[21], buf4[21];

    TTY::putString("[DBG] keyboardGSI: ", TTY::Color::CYAN);
    TTY::putString(uint64ToString(keyboardGSI, buf1, 21), TTY::Color::YELLOW);
    TTY::putString("\n", TTY::Color::WHITE);

    TTY::putString("[DBG] keyboardFlags: ", TTY::Color::CYAN);
    TTY::putString(uint64ToString(keyboardFlags, buf2, 21), TTY::Color::YELLOW);
    TTY::putString("\n", TTY::Color::WHITE);

    TTY::putString("[DBG] lowBits: ", TTY::Color::CYAN);
    uint64ToHex(lowBits, buf3);
    TTY::putString(buf3, TTY::Color::YELLOW);
    TTY::putString("\n", TTY::Color::WHITE);

    TTY::putString("[DBG] highBits: ", TTY::Color::CYAN);
    uint64ToHex(highBits, buf4);
    TTY::putString(buf4, TTY::Color::YELLOW);
    TTY::putString("\n", TTY::Color::WHITE);

    writeIOAPIC(IOAPICREDTBL(keyboardGSI), lowBits);
    writeIOAPIC(IOAPICREDTBL(keyboardGSI) + 1, highBits);

    // Drain pending scancodes
    while (IO::Port::inb(0x64) & 0x1)
        IO::Port::inb(0x60);

    CPU::APIC::LAPICEOI();
}