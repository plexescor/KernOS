#pragma once
#include <cstdint>
namespace CPU
{
    namespace APIC
    {
        void disableLegacyPIC();
        void enableLAPICGlobally();
        bool checkLAPICSupport();
        bool initialiseLAPIC();
        void LAPICEOI();

        uint32_t readIOAPIC(uint8_t offset);
        void writeIOAPIC(uint8_t offset, uint32_t value);
        bool initialiseIOAPIC();
        void IOAPICSetupKeyboard(uint8_t IDTVector, uint8_t LAPICID);
    } // APIC
} // CPU
