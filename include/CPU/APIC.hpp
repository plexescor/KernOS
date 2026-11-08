#pragma once

namespace CPU
{
    namespace APIC
    {
        void disableLegacyPIC();
        void enableLAPICGlobally();
        bool checkAPICSupport();
        bool initialiseAPIC();
        void LAPICEOI();
    } // APIC
} // CPU
