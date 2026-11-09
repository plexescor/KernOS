#include "IO/Keyboard.hpp"
#include "IO/PortIO.hpp"
#include "CPU/APIC.hpp"
#include "TTY/TTY.hpp"

#include <cstdint>

#define KEYBOARD_IO_PORT 0x60
static bool shiftPressed = false;
static bool capslockPressed = false;
static bool extendedMode = false; // Not used currently :)

static const char ASCIIMap_Normal[] =
{
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0,
  '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0, '*',   0, ' '
};

static const char ASCIIMap_Shift[] =
{
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
  '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',   0,
  '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',   0, '*',   0, ' '
};

char IO::Keyboard::translateScanCode(uint8_t code)
{
    // Break == Key unpress
    bool isBreak = (code & 0x80) != 0;
    uint8_t cleanCode = code & ~0x80;

    // L and R shift
    if (cleanCode == 0x2A || cleanCode == 0x36)
    {
        shiftPressed = !isBreak;
        return '\0';
    }

    // Capslock
    if (cleanCode == 0x3A && isBreak)
    {
        capslockPressed = !capslockPressed;
        return '\0';
    }

    if (isBreak)
    {
        return '\0';
    }

    if (cleanCode >= sizeof(ASCIIMap_Normal))
    {
        return '\0';
    }

    char ASCII = '\0';
    bool useUppercase = (shiftPressed ^ capslockPressed);

    char baseChar = ASCIIMap_Normal[cleanCode];
    if (baseChar >= 'a' && baseChar <= 'z')
    {
        ASCII = useUppercase ? ASCIIMap_Shift[cleanCode] : baseChar;
    }
    else
    {
        ASCII = shiftPressed ? ASCIIMap_Shift[cleanCode] : baseChar;
    }
    return ASCII;
}

void IO::Keyboard::handleKeyboardInterrupt()
{
    // Get the scancode from the hardware port
    uint8_t scanCode = Port::inb(KEYBOARD_IO_PORT);
    // If its not a release scanCode
    char c = IO::Keyboard::translateScanCode(scanCode);
    char translated[2] = { c, '\0' };
    TTY::putString(translated, 0xFFFFFF, 0x000000);

    // Tell we handled it
    CPU::APIC::LAPICEOI();
}
