#include "terminal/terminal_image.h"
#include "terminal/terminal_style.h"
#include "c23/constructor.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** Image → cells. A TerminalImage borrows decoded RGBA8 pixels and rasterises
 * them into a canvas region: each cell averages a box of source pixels into a
 * luminance, picks a glyph from a character ramp, and stamps the image's style
 * flags. Owns no memory, decodes nothing, names no terminal. An application's
 * badge or wallpaper is just one such image; the toolkit ships no art. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalImage (terminal/terminal_image.c)
 * STRUCT FIELDS (Mirroring terminal/terminal_image.h)
 * const uint8_t *pixels  borrowed RGBA8 row-major source
 * size_t width, height   source pixel extent
 * const char *ramp       glyph ladder low→high (default " .:-=+*#%@")
 * uint8_t style          style flags on drawn cells (default DIM)
 * bool color             carry the sampled colour as foreground (default true)
 * bool skipTransparent   leave alpha==0 cells untouched
 * bool invert            bright = low luminance
 * FUNCTION REGISTRY (exports)
 * Constructors: TerminalImage_0(void), TerminalImage_3(pixels, width, height)
 * Config: setPixels/getPixels, set/getRamp, set/getStyle,
 *   setColor/hasColor, setSkipTransparent/skipsTransparent, setInvert/inverted
 * Queries: bool TerminalImage_isValid, getWidth/getHeight
 * Mapping: uint8_t TerminalImage_luminance(r,g,b)
 *   char TerminalImage_glyph(self, luminance)
 *   bool TerminalImage_sampleCell(self, lx, ly, outW, outH, outR/G/B/A)
 * Render: TerminalImage_render(self, canvas, x, y)      (native 1:1)
 *   TerminalImage_renderFit(self, canvas)               (cover the region)
 * Projections: bool TerminalImage_toString/toStringStruct(...)
 * CONTRACT: an invalid image (null/zero pixels or extent) draws nothing; a
 * transparent cell with skipTransparent leaves the canvas untouched; render
 * clips and never allocates or logs. Sampling is pure and deterministic.
 * ======================================================================== */

TerminalImage TerminalImage_0(void) {
    TerminalImage image = {0};
    image.ramp = TERMINAL_IMAGE_RAMP_DEFAULT;
    image.style = TERMINAL_STYLE_DIM;
    image.color = true;
    image.skipTransparent = true;
    return image;
}

TerminalImage TerminalImage_3(const uint8_t *pixels, size_t width, size_t height) {
    TerminalImage image = TerminalImage_0();
    image.pixels = (width != 0 && height != 0) ? pixels : nullptr;
    image.width = (pixels != nullptr) ? width : 0;
    image.height = (pixels != nullptr) ? height : 0;
    return image;
}

bool TerminalImage_setPixels(TerminalImage *self, const uint8_t *pixels, size_t width, size_t height) {
    if (self == nullptr || pixels == nullptr || width == 0 || height == 0) {
        THROW("TerminalImage pixels: null or zero extent");
        return false;
    }
    (*self).pixels = pixels;
    (*self).width = width;
    (*self).height = height;
    return true;
}

const uint8_t *TerminalImage_getPixels(const TerminalImage *self) { return self ? (*self).pixels : nullptr; }
size_t TerminalImage_getWidth(const TerminalImage *self) { return self ? (*self).width : 0; }
size_t TerminalImage_getHeight(const TerminalImage *self) { return self ? (*self).height : 0; }
bool TerminalImage_isValid(const TerminalImage *self) {
    return self != nullptr && (*self).pixels != nullptr && (*self).width > 0 && (*self).height > 0;
}

bool TerminalImage_setRamp(TerminalImage *self, const char *ramp) {
    if (self == nullptr || ramp == nullptr || ramp[0] == '\0') {
        THROW("TerminalImage ramp: null or empty");
        return false;
    }
    (*self).ramp = ramp;
    return true;
}
const char *TerminalImage_getRamp(const TerminalImage *self) {
    return (self && (*self).ramp) ? (*self).ramp : TERMINAL_IMAGE_RAMP_DEFAULT;
}
void TerminalImage_setStyle(TerminalImage *self, uint8_t style) { if (self != nullptr) (*self).style = style; }
uint8_t TerminalImage_getStyle(const TerminalImage *self) { return self ? (*self).style : TERMINAL_STYLE_NONE; }
void TerminalImage_setColor(TerminalImage *self, bool color) { if (self != nullptr) (*self).color = color; }
bool TerminalImage_hasColor(const TerminalImage *self) { return self && (*self).color; }
void TerminalImage_setSkipTransparent(TerminalImage *self, bool skip) { if (self != nullptr) (*self).skipTransparent = skip; }
bool TerminalImage_skipsTransparent(const TerminalImage *self) { return self && (*self).skipTransparent; }
void TerminalImage_setInvert(TerminalImage *self, bool invert) { if (self != nullptr) (*self).invert = invert; }
bool TerminalImage_inverted(const TerminalImage *self) { return self && (*self).invert; }

uint8_t TerminalImage_luminance(uint8_t r, uint8_t g, uint8_t b) {
    // Rec. 709 luma on 8-bit channels, integer arithmetic (deterministic).
    return (uint8_t) ((2126u * r + 7152u * g + 722u * b) / 10000u);
}

char TerminalImage_glyph(const TerminalImage *self, uint8_t luminance) {
    const char *ramp = TerminalImage_getRamp(self);
    size_t levels = strlen(ramp);
    if (levels == 0)
        return ' ';
    bool invert = TerminalImage_inverted(self);
    size_t index = (size_t) luminance * levels / 256u;
    if (index >= levels)
        index = levels - 1;
    if (invert)
        index = levels - 1 - index;
    return ramp[index];
}

bool TerminalImage_sampleCell(const TerminalImage *self, size_t lx, size_t ly,
                              size_t outWidth, size_t outHeight,
                              uint8_t *outR, uint8_t *outG, uint8_t *outB, uint8_t *outA) {
    if (!TerminalImage_isValid(self) || outWidth == 0 || outHeight == 0)
        return false;
    size_t x0 = lx * (*self).width / outWidth;
    size_t x1 = (lx + 1) * (*self).width / outWidth;
    size_t y0 = ly * (*self).height / outHeight;
    size_t y1 = (ly + 1) * (*self).height / outHeight;
    if (x1 <= x0) x1 = x0 + 1;
    if (y1 <= y0) y1 = y0 + 1;
    if (x1 > (*self).width) x1 = (*self).width;
    if (y1 > (*self).height) y1 = (*self).height;
    uint64_t sr = 0, sg = 0, sb = 0;
    uint8_t maxA = 0;
    size_t count = 0;
    for (size_t y = y0; y < y1; ++y) {
        for (size_t x = x0; x < x1; ++x) {
            size_t index = (y * (*self).width + x) * 4;
            const uint8_t *p = (*self).pixels + index;
            sr += p[0]; sg += p[1]; sb += p[2];
            if (p[3] > maxA) maxA = p[3];
            ++count;
        }
    }
    if (count == 0)
        return false;
    if (outR) *outR = (uint8_t) (sr / count);
    if (outG) *outG = (uint8_t) (sg / count);
    if (outB) *outB = (uint8_t) (sb / count);
    if (outA) *outA = maxA;
    return true;
}

void TerminalImage_render(const TerminalImage *self, TerminalCanvas *canvas, size_t x, size_t y) {
    if (!TerminalImage_isValid(self) || !TerminalCanvas_isValid(canvas))
        return;
    size_t cw = TerminalCanvas_getWidth(canvas);
    size_t chh = TerminalCanvas_getHeight(canvas);
    for (size_t ly = 0; ly < (*self).height; ++ly) {
        if (y + ly >= chh)
            break;
        for (size_t lx = 0; lx < (*self).width; ++lx) {
            if (x + lx >= cw)
                break;
            size_t index = (ly * (*self).width + lx) * 4;
            const uint8_t *p = (*self).pixels + index;
            if (TerminalImage_skipsTransparent(self) && p[3] == 0)
                continue;
            char glyph = TerminalImage_glyph(self, TerminalImage_luminance(p[0], p[1], p[2]));
            TerminalPen pen = TerminalPen_2((*self).style,
                                            (*self).color ? TERMINAL_RGB(p[0], p[1], p[2]) : TERMINAL_COLOR_DEFAULT);
            TerminalCanvas_text(canvas, x + lx, y + ly, &glyph, 1, pen);
        }
    }
}

void TerminalImage_renderFit(const TerminalImage *self, TerminalCanvas *canvas) {
    if (!TerminalImage_isValid(self) || !TerminalCanvas_isValid(canvas))
        return;
    size_t cw = TerminalCanvas_getWidth(canvas);
    size_t chh = TerminalCanvas_getHeight(canvas);
    for (size_t ly = 0; ly < chh; ++ly) {
        for (size_t lx = 0; lx < cw; ++lx) {
            uint8_t r = 0, g = 0, b = 0, a = 0;
            if (!TerminalImage_sampleCell(self, lx, ly, cw, chh, &r, &g, &b, &a))
                continue;
            if (TerminalImage_skipsTransparent(self) && a == 0)
                continue;
            char glyph = TerminalImage_glyph(self, TerminalImage_luminance(r, g, b));
            TerminalPen pen = TerminalPen_2((*self).style,
                                            (*self).color ? TERMINAL_RGB(r, g, b) : TERMINAL_COLOR_DEFAULT);
            TerminalCanvas_text(canvas, lx, ly, &glyph, 1, pen);
        }
    }
}

bool TerminalImage_toString(const TerminalImage *self, char *dest, size_t cap, bool *outTruncated) {
    if (outTruncated != nullptr)
        *outTruncated = false;
    if (dest == nullptr || cap == 0) {
        if (outTruncated != nullptr)
            *outTruncated = true;
        THROW("TerminalImage projection: missing destination");
        return false;
    }
    int n = self ? snprintf(dest, cap,
        "TerminalImage{%zux%zu,ramp=\"%s\",style=0x%02X,color=%d,skipTransparent=%d,invert=%d,valid=%d}",
        (*self).width, (*self).height, TerminalImage_getRamp(self), (*self).style,
        (*self).color, (*self).skipTransparent, (*self).invert, TerminalImage_isValid(self))
        : snprintf(dest, cap, "nullptr");
    bool cut = n < 0 || (size_t) n >= cap;
    if (outTruncated != nullptr)
        *outTruncated = cut;
    if (cut)
        THROW("TerminalImage projection: truncated at %zu bytes", cap);
    return !cut;
}

bool TerminalImage_toStringStruct(const TerminalImage *self, char *dest, size_t cap, bool *outTruncated) {
    return TerminalImage_toString(self, dest, cap, outTruncated);
}
