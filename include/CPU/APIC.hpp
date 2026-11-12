#pragma once
#include "Memory/VirtualMemoryManager.hpp"
#include <cstdint>
namespace CPU
{
    namespace APIC
    {
        void disableLegacyPIC();
        void mapMMIO(uint64_t physBase, uint64_t size);
        void enableLAPICGlobally();
        bool checkLAPICSupport();
        bool initialiseLAPIC(uint64_t LAPICBase_);
        void LAPICEOI();

        uint32_t readIOAPIC(uint8_t offset);
        void initPS2();
        uint8_t getLAPICID();
        void writeIOAPIC(uint8_t offset, uint32_t value);
        bool initialiseIOAPIC(uint64_t address, uint32_t keyboardGSI_P, uint16_t keyboardFlags_P);
        void IOAPICSetupKeyboard(uint8_t IDTVector, uint8_t LAPICID);
    } // APIC
} // CPU
