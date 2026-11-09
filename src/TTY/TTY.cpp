#include "TTY/TTY.hpp"
#include "limine.h"
#include "Fonts/PSFfonts.hpp"
#include "Framebuffer/Framebuffer.hpp"

#include <cstdint>
#include <cstddef>

extern char _binary_Fonts_Font_psf_start[];
extern char _binary_Fonts_Font_psf_end[];
extern char _binary_Fonts_Font_psf_size[];

#define PIXEL uint32_t

void TTY::putChar(uint16_t c, size_t cX, size_t cY, uint32_t fg, uint32_t bg)
{
    PSF_font *font = (PSF_font*)&_binary_Fonts_Font_psf_start;

    uint8_t* glyph =
        (uint8_t*)&_binary_Fonts_Font_psf_start +
        font->headersize +
        (c>0&&c<font->numglyph?c:0)*font->bytesperglyph;
    /* calculate the upper left corner on screen where we want to display.
       we only do this once, and adjust the offset later. This is faster. */

	// Hardcode the index for now
	struct limine_framebuffer* fb = Framebuffer::getFramebuffer(0);
	uint64_t pitch = Framebuffer::getFramebufferPitch(fb);
	uint8_t* base = (uint8_t*)fb->address;
    int offsets =
        (cY * font->height * pitch) +
        (cX * (font->width + 1) * sizeof(PIXEL));

	uint32_t bytesPerGlyphLine = (font->width + 7) / 8;

	uint32_t x, y, line;
    for(y = 0; y < font->height; y++){
        /* save the starting position of the line */
        line = offsets;
        /* Calulate where the first byte for this line of the glyph is. */
        uint8_t* currentByte = glyph + (bytesPerGlyphLine * y);
        /* Start with a mask at this byte's MSB */
        uint8_t mask = 1<< 7;
        /* display a row */
        for(x = 0; x <font->width; x++)
		{
            *((PIXEL*)(base + line)) = (*currentByte & mask) ? fg : bg;
            mask >>= 1;
            if (mask == 0){
                /* We have read this byte of the glyph.
                Reset mask and move to next byte */
                mask = 1<<7;
                currentByte += 1;
            }
            /* adjust to the next pixel in framebuffer */
            line += sizeof(PIXEL);
        }
        /* adjust to the next line in framebuffer */
        offsets  += pitch;
    }
}

static size_t y = 0;
static size_t x = 0;
void TTY::putString(const char* str,  uint32_t fg, uint32_t bg)
{
    while (*str)
    {
        if (*str == '\n')
        {
            y++;          // move down one character row
            x = 0;  // reset to left margin
        }
        else
        {
            TTY::putChar((uint16_t)*str, x, y, fg, bg);
            x++;
        }
        str++;
    }
}
