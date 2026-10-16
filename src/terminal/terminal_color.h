#ifndef DETERMINANT_TERMINAL_COLOR_H
#define DETERMINANT_TERMINAL_COLOR_H

#include <stdint.h>

// Determinant terminal colour — packed 0xRRGGBBAA, the vexgraph-ecosystem-gitrepo's Strict
// 0xRRGGBBAA Color Law applied to cells. Red is the high byte, alpha the low
// byte, so a hex literal reads R,G,B,A left to right and the same value means
// the same thing in a terminal cell, a graphvex Image or a brush.
//
// TERMINAL_COLOR_DEFAULT (fully transparent, 0x00000000) is the "unset" sentinel:
// the device backend renders its own default fg/bg. Opaque black is
// 0x000000FF and is NOT the sentinel, so black text is expressible.

#define TERMINAL_COLOR_DEFAULT 0x00000000u

#define TERMINAL_RGBA(r, g, b, a) \
    ((((uint32_t) (r) & 0xFFu) << 24) | (((uint32_t) (g) & 0xFFu) << 16) | \
     (((uint32_t) (b) & 0xFFu) << 8) | ((uint32_t) (a) & 0xFFu))
#define TERMINAL_RGB(r, g, b) TERMINAL_RGBA(r, g, b, 0xFFu)

#define TERMINAL_COLOR_RED(c)   ((uint8_t) (((c) >> 24) & 0xFFu))
#define TERMINAL_COLOR_GREEN(c) ((uint8_t) (((c) >> 16) & 0xFFu))
#define TERMINAL_COLOR_BLUE(c)  ((uint8_t) (((c) >> 8) & 0xFFu))
#define TERMINAL_COLOR_ALPHA(c) ((uint8_t) ((c) & 0xFFu))
// A colour is "set" when its alpha is non-zero; the default has alpha 0.
#define TERMINAL_COLOR_IS_SET(c) (((c) & 0xFFu) != 0u)

#endif
