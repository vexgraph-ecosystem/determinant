#include "ui/image/terminal_image.h"
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
 * CLASS: TerminalImage (ui/image/terminal_image.c)
 * STRUCT FIELDS (Mirroring ui/image/terminal_image.h)
 * const uint8_t *pixels  borrowed RGBA8 row-major source
 * size_t width, height   source pixel extent
 * const char *ramp       glyph ladder low→high (default " .:-=+*#%@")
 * uint8_t style          style flags on drawn cells (default DIM)
 * bool color             carry the sampled colour as foreground (default true)
 * const uint32_t *colorRamp; size_t colorRampCount  palette treatment (nullable)
 * uint8_t jitter; uint32_t jitterSeed  deterministic per-cell shade scatter
 * bool skipTransparent   leave alpha==0 cells untouched
 * bool invert            bright = low luminance
 * FUNCTION REGISTRY (exports)
 * Constructors: TerminalImage_0(void), TerminalImage_3(pixels, width, height)
 * Config: setPixels/getPixels, set/getRamp, set/getStyle,
 *   setColor/hasColor, setColorRamp/getColorRamp, setJitter/getJitter,
 *   rampPurple, setSkipTransparent/skipsTransparent, setInvert/inverted
 * Queries: bool TerminalImage_isValid, getWidth/getHeight
 * Mapping: uint8_t TerminalImage_luminance(r,g,b)
 *   char TerminalImage_glyph(self, luminance)
 *   TerminalImageSample TerminalImage_sampleCell(self, lx, ly, outW, outH)
 * Render: TerminalImage_render(self, canvas, x, y)      (native 1:1)
 *   TerminalImage_renderFit(self, canvas)               (cover the region)
 * Projections: TerminalProjection TerminalImage_toString/toStringStruct(self)
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
    image.cellAspect = 2.0;   // a terminal cell is ~2x taller than wide
    image.noiseDensity = 100; // full cover until the caller thins it
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

bool TerminalImage_setColorRamp(TerminalImage *self, const uint32_t *ramp, size_t count) {
    if (self == nullptr || (count != 0 && ramp == nullptr)) {
        THROW("TerminalImage color ramp: null");
        return false;
    }
    (*self).colorRamp = (count != 0) ? ramp : nullptr;
    (*self).colorRampCount = (count != 0) ? count : 0;
    return true;
}
TerminalImageColorRamp TerminalImage_getColorRamp(const TerminalImage *self) {
    return self ? (TerminalImageColorRamp) { (*self).colorRamp, (*self).colorRampCount }
                : (TerminalImageColorRamp) { nullptr, 0u };
}
void TerminalImage_setJitter(TerminalImage *self, uint8_t jitter, uint32_t seed) {
    if (self == nullptr) return;
    (*self).jitter = jitter;
    (*self).jitterSeed = seed;
}
uint8_t TerminalImage_getJitter(const TerminalImage *self) { return self ? (*self).jitter : 0; }
uint32_t TerminalImage_getJitterSeed(const TerminalImage *self) { return self ? (*self).jitterSeed : 0; }
void TerminalImage_setPaletteScatter(TerminalImage *self, uint8_t steps) { if (self != nullptr) (*self).paletteScatter = steps; }
uint8_t TerminalImage_getPaletteScatter(const TerminalImage *self) { return self ? (*self).paletteScatter : 0; }

// A purple ladder for a light terminal: the darkest entry sits at ~30% of full
// range (0x4C ≈ 255 * 0.3) and the rest climb to near-white. Which cell takes
// which entry is the caller's choice — luminance order, or scattered (see
// paletteScatter) so light and dark letters interleave at random.
static const uint32_t PURPLE_RAMP[] = {
    TERMINAL_RGB(0x5A, 0x4C, 0x78),
    TERMINAL_RGB(0x6E, 0x5A, 0x90),
    TERMINAL_RGB(0x82, 0x6B, 0xA8),
    TERMINAL_RGB(0x9A, 0x7E, 0xC0),
    TERMINAL_RGB(0xB2, 0x92, 0xD4),
    TERMINAL_RGB(0xCA, 0xAA, 0xE6),
    TERMINAL_RGB(0xE0, 0xC6, 0xF4),
    TERMINAL_RGB(0xF2, 0xE6, 0xFF),
};
TerminalImageColorRamp TerminalImage_rampPurple(void) {
    return (TerminalImageColorRamp) { PURPLE_RAMP, sizeof(PURPLE_RAMP) / sizeof(PURPLE_RAMP[0]) };
}

static uint8_t shiftChannel(uint8_t value, int delta) {
    int shifted = (int) value + delta;
    if (shifted < 0) shifted = 0;
    if (shifted > 255) shifted = 255;
    return (uint8_t) shifted;
}

// The colour one cell draws with: palette (with deterministic jitter) or the
// sampled colour, or none.
static uint32_t imageColor(const TerminalImage *self, size_t lx, size_t ly,
                           uint8_t r, uint8_t g, uint8_t b) {
    if ((*self).colorRamp != nullptr && (*self).colorRampCount > 0) {
        uint8_t lum = TerminalImage_luminance(r, g, b);
        size_t index = (size_t) lum * (*self).colorRampCount / 256u;
        if (index >= (*self).colorRampCount)
            index = (*self).colorRampCount - 1;
        uint32_t hash = (uint32_t) lx * 73856093u ^ (uint32_t) ly * 19349663u ^ (*self).jitterSeed;
        if ((*self).paletteScatter != 0) {
            // Which shade a cell takes is randomized, not luminance-ordered, so
            // light and dark entries interleave across the picture.
            index = (index + (hash >> 8) % (*self).paletteScatter) % (*self).colorRampCount;
        }
        uint32_t colour = (*self).colorRamp[index];
        if ((*self).jitter != 0) {
            // The jitter only ever LIGHTENS a palette colour (delta >= 0), so a
            // light ramp never dips dark on some cells.
            int delta = (int) ((hash >> 8) % ((uint32_t) (*self).jitter + 1u));
            return TERMINAL_RGBA(shiftChannel(TERMINAL_COLOR_RED(colour), delta),
                                 shiftChannel(TERMINAL_COLOR_GREEN(colour), delta),
                                 shiftChannel(TERMINAL_COLOR_BLUE(colour), delta),
                                 TERMINAL_COLOR_ALPHA(colour));
        }
        return colour;
    }
    return (*self).color ? TERMINAL_RGB(r, g, b) : TERMINAL_COLOR_DEFAULT;
}
void TerminalImage_setSkipTransparent(TerminalImage *self, bool skip) { if (self != nullptr) (*self).skipTransparent = skip; }
bool TerminalImage_skipsTransparent(const TerminalImage *self) { return self && (*self).skipTransparent; }
void TerminalImage_setInvert(TerminalImage *self, bool invert) { if (self != nullptr) (*self).invert = invert; }
bool TerminalImage_inverted(const TerminalImage *self) { return self && (*self).invert; }
void TerminalImage_setCellAspect(TerminalImage *self, double aspect) {
    if (self != nullptr && aspect > 0.0)
        (*self).cellAspect = aspect;
}
double TerminalImage_getCellAspect(const TerminalImage *self) {
    return (self && (*self).cellAspect > 0.0) ? (*self).cellAspect : 2.0;
}

bool TerminalImage_setNoise(TerminalImage *self, const char *glyphs, uint32_t color) {
    if (self == nullptr || glyphs == nullptr || glyphs[0] == '\0') {
        if (self != nullptr) {
            (*self).noise = nullptr;
            (*self).noiseSeed = 0;
        }
        return false;
    }
    (*self).noise = glyphs;
    (*self).noiseColor = color;
    return true;
}
const char *TerminalImage_getNoise(const TerminalImage *self) { return self ? (*self).noise : nullptr; }
void TerminalImage_setNoiseSeed(TerminalImage *self, uint32_t seed) { if (self != nullptr) (*self).noiseSeed = seed; }
uint32_t TerminalImage_getNoiseSeed(const TerminalImage *self) { return self ? (*self).noiseSeed : 0; }
void TerminalImage_setNoiseStyle(TerminalImage *self, uint8_t style) { if (self != nullptr) (*self).noiseStyle = style; }
uint8_t TerminalImage_getNoiseStyle(const TerminalImage *self) { return self ? (*self).noiseStyle : 0; }
void TerminalImage_setNoiseDensity(TerminalImage *self, uint8_t percent) {
    if (self != nullptr)
        (*self).noiseDensity = percent > 100 ? 100 : percent;
}
uint8_t TerminalImage_getNoiseDensity(const TerminalImage *self) {
    return self ? (*self).noiseDensity : 0;
}

// The background NOISE field: fill a whole rectangle with glyphs from the
// caller's alphabet, gated by density and scattered by the seed. This is a
// DECOR FIELD sized by the region (never by the picture), so it fills the panel
// at any terminal size. Deterministic for a fixed seed.
static void paintNoiseField(const TerminalImage *self, TerminalCanvas *canvas,
                            size_t x0, size_t y0, size_t w, size_t h) {
    const char *noise = (*self).noise;
    if (noise == nullptr || (*self).noiseDensity == 0)
        return;
    size_t count = strlen(noise);
    if (count == 0)
        return;
    uint32_t colour = (*self).noiseColor;
    if (colour == TERMINAL_COLOR_DEFAULT)
        colour = (*self).colorRamp != nullptr ? (*self).colorRamp[0] : TERMINAL_COLOR_DEFAULT;
    TerminalPen pen = TerminalPen_2((*self).noiseStyle, colour);
    for (size_t ly = 0; ly < h; ++ly) {
        for (size_t lx = 0; lx < w; ++lx) {
            uint32_t hash = (uint32_t) (x0 + lx) * 2654435761u
                          ^ (uint32_t) (y0 + ly) * 40503u
                          ^ (*self).noiseSeed;
            hash ^= hash >> 13;
            hash *= 2654435761u;
            hash ^= hash >> 15;
            if ((hash % 100u) >= (*self).noiseDensity)
                continue;
            char glyph = noise[(hash >> 7) % count];
            TerminalCanvas_text(canvas, x0 + lx, y0 + ly, &glyph, 1, pen);
        }
    }
}

// Paint ONE cell of the picture. When a noise field is set the blank background
// is left alone (the field underneath shows through) and only real content is
// drawn on top.
static void paintCell(const TerminalImage *self, TerminalCanvas *canvas,
                      size_t cx, size_t cy, size_t lx, size_t ly,
                      uint8_t r, uint8_t g, uint8_t b) {
    char glyph = TerminalImage_glyph(self, TerminalImage_luminance(r, g, b));
    if ((*self).noise != nullptr) {
        const char *ramp = TerminalImage_getRamp(self);
        if (glyph == ramp[0])
            return;   // the decor field shows through
    }
    TerminalCanvas_text(canvas, cx, cy, &glyph, 1,
                        TerminalPen_2((*self).style, imageColor(self, lx, ly, r, g, b)));
}

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

TerminalImageSample TerminalImage_sampleCell(const TerminalImage *self, size_t lx, size_t ly,
                                             size_t outWidth, size_t outHeight) {
    TerminalImageSample sample = {0};
    if (!TerminalImage_isValid(self) || outWidth == 0 || outHeight == 0)
        return sample;
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
        return sample;
    sample.valid = true;
    sample.red = (uint8_t) (sr / count);
    sample.green = (uint8_t) (sg / count);
    sample.blue = (uint8_t) (sb / count);
    sample.alpha = maxA;
    return sample;
}

void TerminalImage_render(const TerminalImage *self, TerminalCanvas *canvas, size_t x, size_t y) {
    if (!TerminalImage_isValid(self) || !TerminalCanvas_isValid(canvas))
        return;
    size_t cw = TerminalCanvas_getWidth(canvas);
    size_t chh = TerminalCanvas_getHeight(canvas);
    paintNoiseField(self, canvas, x, y, (*self).width, (*self).height);
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
            paintCell(self, canvas, x + lx, y + ly, x + lx, y + ly, p[0], p[1], p[2]);
        }
    }
}

void TerminalImage_renderFit(const TerminalImage *self, TerminalCanvas *canvas) {
    if (!TerminalImage_isValid(self) || !TerminalCanvas_isValid(canvas))
        return;
    size_t cw = TerminalCanvas_getWidth(canvas);
    size_t chh = TerminalCanvas_getHeight(canvas);
    paintNoiseField(self, canvas, 0, 0, cw, chh);
    for (size_t ly = 0; ly < chh; ++ly) {
        for (size_t lx = 0; lx < cw; ++lx) {
            TerminalImageSample sample = TerminalImage_sampleCell(self, lx, ly, cw, chh);
            if (!sample.valid)
                continue;
            if (TerminalImage_skipsTransparent(self) && sample.alpha == 0)
                continue;
            paintCell(self, canvas, lx, ly, lx, ly, sample.red, sample.green, sample.blue);
        }
    }
}

void TerminalImage_renderContain(const TerminalImage *self, TerminalCanvas *canvas) {
    if (!TerminalImage_isValid(self) || !TerminalCanvas_isValid(canvas))
        return;
    size_t regionW = TerminalCanvas_getWidth(canvas);
    size_t regionH = TerminalCanvas_getHeight(canvas);
    if (regionW == 0 || regionH == 0)
        return;
    // One scale for both axes, so the picture is never squashed; the cells are
    // ~2x taller than wide, so the vertical extent is corrected by cellAspect.
    double aspect = (*self).cellAspect > 0.0 ? (*self).cellAspect : 1.0;
    double widthPerHeight = (double) (*self).width / (double) (*self).height * aspect;
    size_t destW = regionW;
    size_t destH = (size_t) ((double) regionW / widthPerHeight + 0.5);
    if (destH > regionH) {
        destH = regionH;
        destW = (size_t) ((double) regionH * widthPerHeight + 0.5);
    }
    if (destW == 0 || destH == 0)
        return;
    if (destW > regionW) destW = regionW;
    if (destH > regionH) destH = regionH;
    size_t offsetX = (regionW - destW) / 2;
    size_t offsetY = (regionH - destH) / 2;
    // The decor field covers the WHOLE region, not just the picture's fit rect,
    // so the panel is filled edge to edge at any terminal size.
    paintNoiseField(self, canvas, 0, 0, regionW, regionH);
    for (size_t ly = 0; ly < destH; ++ly) {
        for (size_t lx = 0; lx < destW; ++lx) {
            TerminalImageSample sample = TerminalImage_sampleCell(self, lx, ly, destW, destH);
            if (!sample.valid)
                continue;
            if (TerminalImage_skipsTransparent(self) && sample.alpha == 0)
                continue;
            paintCell(self, canvas, offsetX + lx, offsetY + ly, lx, ly,
                      sample.red, sample.green, sample.blue);
        }
    }
}

TerminalProjection TerminalImage_toString(const TerminalImage *self) {
    return TerminalImage_toStringStruct(self);
}

TerminalProjection TerminalImage_toStringStruct(const TerminalImage *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalImage{%zux%zu,ramp=\"%s\",style=0x%02X,color=%d,palette=%zu,jitter=%u,skipTransparent=%d,invert=%d,valid=%d}",
        (*self).width, (*self).height, TerminalImage_getRamp(self), (*self).style,
        (*self).color, (*self).colorRampCount, (*self).jitter, (*self).skipTransparent, (*self).invert,
        TerminalImage_isValid(self));
}
