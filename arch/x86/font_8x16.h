#ifndef _JLOS_ARCH_X86_FONT_8X16_H
#define _JLOS_ARCH_X86_FONT_8X16_H

#include <common/types.h>

#define JLOS_FONT_GLYPH_HEIGHT  16
#define JLOS_FONT_GLYPH_WIDTH   8
#define JLOS_FONT_GLYPH_COUNT   256

extern const uint8_t jlos_font_8x16[JLOS_FONT_GLYPH_COUNT][JLOS_FONT_GLYPH_HEIGHT];

#endif
