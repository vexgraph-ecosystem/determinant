#ifndef DETERMINANT_TERMINAL_IMAGE_H
#define DETERMINANT_TERMINAL_IMAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "terminal/terminal_canvas.h"
#include "terminal/terminal_color.h"
#include "c23/constructor.h"

// terminal/terminal_image.h — turn a picture into cells.
//
// A TerminalImage borrows raw RGBA8 pixels and rasterises them into a
// TerminalCanvas region: each output cell samples a box of source pixels,
// reduces them to a luminance, and picks a glyph from a character RAMP
// (low→high, e.g. " .:-=+*#%@"). The drawn cell carries the image's style flags
// (DIM by default), so the same picture can render crisp or ghost-faint.
//
// This is the general "image → canvas" capability. It owns NO memory and does
// NO decoding: the caller supplies decoded pixels (a PNG/BMP decoder, a
// framebuffer, a graphvex Image, or a small constant art table all work). It
// never names a terminal, never writes to a device, and never allocates.
//
// An application's badge or wallpaper is exactly this: an app-owned image
// rendered through the toolkit. The toolkit ships no art of its own.

#define TERMINAL_IMAGE_RAMP_DEFAULT " .:-=+*#%@"

typedef struct TerminalImage {
    const uint8_t *pixels;  // borrowed RGBA8, row-major: width*height*4 bytes
    size_t width;           // source pixel columns
    size_t height;          // source pixel rows
    const char *ramp;       // glyph ladder low→high (default " .:-=+*#%@")
    uint8_t style;          // style flags applied to drawn cells (default DIM)
    bool color;             // carry the sampled colour as the cell foreground
    bool skipTransparent;   // leave alpha==0 cells untouched (default true)
    bool invert;            // treat bright as low luminance (default false)
} TerminalImage;

TerminalImage TerminalImage_0(void);
TerminalImage TerminalImage_3(const uint8_t *pixels, size_t width, size_t height);
#define TerminalImage(...) CONSTRUCTOR_DISPATCH(TerminalImage, __VA_ARGS__)
#define TerminalImage_zero() TerminalImage_0()

bool        TerminalImage_setPixels(TerminalImage *self, const uint8_t *pixels, size_t width, size_t height);
const uint8_t *TerminalImage_getPixels(const TerminalImage *self);
size_t      TerminalImage_getWidth(const TerminalImage *self);
size_t      TerminalImage_getHeight(const TerminalImage *self);
bool        TerminalImage_isValid(const TerminalImage *self);
bool        TerminalImage_setRamp(TerminalImage *self, const char *ramp);
const char *TerminalImage_getRamp(const TerminalImage *self);
void        TerminalImage_setStyle(TerminalImage *self, uint8_t style);
uint8_t     TerminalImage_getStyle(const TerminalImage *self);
// When true (default) each drawn cell carries its sampled colour as the
// foreground (packed 0xRRGGBBAA); when false the image is monochrome shading.
void        TerminalImage_setColor(TerminalImage *self, bool color);
bool        TerminalImage_hasColor(const TerminalImage *self);
void        TerminalImage_setSkipTransparent(TerminalImage *self, bool skip);
bool        TerminalImage_skipsTransparent(const TerminalImage *self);
void        TerminalImage_setInvert(TerminalImage *self, bool invert);
bool        TerminalImage_inverted(const TerminalImage *self);

// Pure helpers (public so a caller/tests can reason about the mapping).
// Rec. 709 luminance from 8-bit channels, 0..255.
uint8_t TerminalImage_luminance(uint8_t r, uint8_t g, uint8_t b);
// The glyph this image maps a luminance to.
char TerminalImage_glyph(const TerminalImage *self, uint8_t luminance);
// Average the source block covering one output cell; alpha is the max of the
// block so a partially transparent cell still reads as opaque.
bool TerminalImage_sampleCell(const TerminalImage *self, size_t lx, size_t ly,
                              size_t outWidth, size_t outHeight,
                              uint8_t *outR, uint8_t *outG, uint8_t *outB, uint8_t *outA);

// Rasterise at native scale (1 source pixel = 1 cell) at (x, y), clipped.
void TerminalImage_render(const TerminalImage *self, TerminalCanvas *canvas, size_t x, size_t y);
// Rasterise to FILL and COVER the whole canvas region (box-downsample when the
// image is larger than the region, nearest when smaller). Deterministic.
void TerminalImage_renderFit(const TerminalImage *self, TerminalCanvas *canvas);

bool TerminalImage_toString(const TerminalImage *self, char *dest, size_t cap, bool *outTruncated);
bool TerminalImage_toStringStruct(const TerminalImage *self, char *dest, size_t cap, bool *outTruncated);

#endif
