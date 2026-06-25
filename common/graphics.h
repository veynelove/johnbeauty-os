#ifndef __JLOS_COMMON_GRAPHICS_H
#define __JLOS_COMMON_GRAPHICS_H

#include <drivers/vga.h>

typedef jlos_vga_t jlos_graphics_context_t;

#define jlos_graphics_context_put_pixel(gc, x, y, r, g, b) jlos_vga_put_pixel(gc, x, y, r, g, b)
#define jlos_graphics_context_fill_rectangle(gc, x, y, w, h, r, g, b) jlos_vga_fill_rectangle(gc, x, y, w, h, r, g, b)

#endif