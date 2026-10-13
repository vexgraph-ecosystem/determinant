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
    // Colour treatment: when colorRamp is set the luminance indexes a PALETTE
    // (a mono mask renders as a coloured mix); `jitter` adds a deterministic
    // per-cell shade variation so the letters do not read as one flat tone.
    const uint32_t *colorRamp;   // low→high packed 0xRRGGBBAA (nullable)
    size_t colorRampCount;
    uint8_t jitter;              // 0..255 per-channel variation (0 = flat)
    uint32_t jitterSeed;         // reproducible scatter
    // Palette SCATTER: instead of following luminance, each cell's palette index
    // is offset by a value drawn from [0, paletteScatter) and wrapped, so light
    // and dark shades of the palette interleave across the picture. 0 = off
    // (luminance order); the ramp's length scatters over the whole palette.
    uint8_t paletteScatter;
    bool skipTransparent;   // leave alpha==0 cells untouched (default true)
    bool invert;            // treat bright as low luminance (default false)
    double cellAspect;      // cell height / width (default 2.0): corrects a
                            // CONTAIN fit so a square source reads square in cells
    // Background NOISE (decor): a glyph field that covers the WHOLE region the
    // image draws into — edge to edge, sized by the region, never by the picture
    // — with the picture's own content painted over it. Every glyph comes from
    // `noise` and is chosen deterministically from (cell, noiseSeed), so
    // advancing the seed (e.g. on every revalidate) re-scatters the decor while a
    // fixed seed stays reproducible. NULL disables the texture.
    const char *noise;      // background glyph alphabet (nullable)
    uint32_t noiseSeed;     // scatter seed; caller advances it to reshuffle
    uint32_t noiseColor;    // packed 0xRRGGBBAA for the noise (DEFAULT = image's own)
    uint8_t noiseStyle;     // style flags for the noise cells
    uint8_t noiseDensity;   // 0..100: percent of the field that is covered
                            // (default 100). Lower reads as a texture behind the
                            // picture instead of a wall in front of it.
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
// Palette treatment: luminance indexes `ramp` (low→high). NULL clears it.
bool        TerminalImage_setColorRamp(TerminalImage *self, const uint32_t *ramp, size_t count);
const uint32_t *TerminalImage_getColorRamp(const TerminalImage *self, size_t *outCount);
// Deterministic per-cell shade scatter (0 = flat). Seed makes it reproducible.
void        TerminalImage_setJitter(TerminalImage *self, uint8_t jitter, uint32_t seed);
uint8_t     TerminalImage_getJitter(const TerminalImage *self);
uint32_t    TerminalImage_getJitterSeed(const TerminalImage *self);
// Palette index scatter: 0 keeps luminance order; N > 0 offsets each cell's
// palette index by [0, N) (wrapped), so which parts read light and which read
// dark is randomized per cell instead of following the gradient.
void        TerminalImage_setPaletteScatter(TerminalImage *self, uint8_t steps);
uint8_t     TerminalImage_getPaletteScatter(const TerminalImage *self);
// Named ramps: a purple mix for a mono mask (the `>w<` badge look).
const uint32_t *TerminalImage_rampPurple(size_t *outCount);
void        TerminalImage_setSkipTransparent(TerminalImage *self, bool skip);
bool        TerminalImage_skipsTransparent(const TerminalImage *self);
void        TerminalImage_setInvert(TerminalImage *self, bool invert);
bool        TerminalImage_inverted(const TerminalImage *self);
// Cell height ÷ width used by the CONTAIN fit (default 2.0 — a terminal cell is
// about twice as tall as it is wide). Set 1.0 for a square-pixel surface.
void        TerminalImage_setCellAspect(TerminalImage *self, double aspect);
double      TerminalImage_getCellAspect(const TerminalImage *self);
// Background NOISE: a decorative glyph field where the picture is blank. The
// alphabet is the caller's (the toolkit ships no art); `color` is the packed
// 0xRRGGBBAA it draws in (TERMINAL_COLOR_DEFAULT uses the image's own colour).
// Returns false for a null/empty alphabet (which clears the noise).
bool        TerminalImage_setNoise(TerminalImage *self, const char *glyphs, uint32_t color);
const char *TerminalImage_getNoise(const TerminalImage *self);
// Advance this to re-scatter the texture — one new value per revalidate gives a
// fresh random look; the same seed reproduces the same scatter exactly.
void        TerminalImage_setNoiseSeed(TerminalImage *self, uint32_t seed);
uint32_t    TerminalImage_getNoiseSeed(const TerminalImage *self);
void        TerminalImage_setNoiseStyle(TerminalImage *self, uint8_t style);
uint8_t     TerminalImage_getNoiseStyle(const TerminalImage *self);
// How much of the blank area is covered, 0..100 (default 100). Below 100 the
// texture reads as a backdrop, leaving the actual picture legible.
void        TerminalImage_setNoiseDensity(TerminalImage *self, uint8_t percent);
uint8_t     TerminalImage_getNoiseDensity(const TerminalImage *self);

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
// Rasterise with a UNIFORM zoom-to-fit: the whole picture is scaled by one
// factor that makes it fit inside the region and is centred, so its aspect and
// orientation never change (a portrait stays portrait, a wide stays wide — it is
// never squashed to the panel). This is the one for a badge or a logo.
void TerminalImage_renderContain(const TerminalImage *self, TerminalCanvas *canvas);

bool TerminalImage_toString(const TerminalImage *self, char *dest, size_t cap, bool *outTruncated);
bool TerminalImage_toStringStruct(const TerminalImage *self, char *dest, size_t cap, bool *outTruncated);

#endif
