#pragma once

#include <cstdint>
namespace IO
{
    namespace Keyboard
    {
        char translateScanCode(uint8_t code);
        void handleKeyboardInterrupt();
    }
}
