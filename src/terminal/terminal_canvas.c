#include "terminal/terminal_canvas.h"
#include "c23/constructor.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** The compositor primitive: a clipped rectangular view over a caller-owned cell
 * grid of four parallel planes (glyph, style, fg, bg). Owns no memory; every
 * widget draws through a region with a TerminalPen and cannot escape it. Regions
 * compose via TerminalCanvas_sub, so a rail, a viewport and a status bar each
 * draw in local coordinates and the layout owner alone decides where they land.
 * All primitives clip and never fault; a fully-outside draw is a silent no-op
 * (composition, not exhaustion). Colour is packed 0xRRGGBBAA; the backend maps
 * it, the toolkit never emits an escape. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalCanvas (terminal/terminal_canvas.c)
 * STRUCT FIELDS (Mirroring terminal/terminal_canvas.h)
 * char *cells                    borrowed row-major glyphs
 * uint8_t *styles                borrowed parallel style plane (nullable)
 * uint32_t *fg, *bg              borrowed parallel colour planes (nullable)
 * size_t gridWidth, gridHeight   full grid extent (indexing stride)
 * size_t x, y, width, height     this region, absolute, clipped to the grid
 * FUNCTION REGISTRY (exports)
 * Pen: TerminalPen_0/_1/_2/_3
 * Constructors: TerminalCanvas_0(void), _4(cells,styles,w,h),
 *   _6(cells,styles,fg,bg,w,h)
 * Queries: bool TerminalCanvas_isValid
 *   size_t getWidth/getHeight
 *   char charAt, uint8_t styleAt, uint32_t fgAt/bgAt
 *   bool hasStylePlane/hasColorPlane
 * Regions: TerminalCanvas TerminalCanvas_sub(self, lx, ly, w, h)
 * Primitives: fill(self, ch, pen), text(self, lx, ly, text, len, pen),
 *   hline(self, ly, ch, pen), vline(self, lx, ch, pen), box(self, pen)
 * Projections: bool copyRow(self, ly, dest, cap, outTruncated)
 *   bool toString/toStringStruct(self, dest, cap, outTruncated)
 * CONTRACT: null/invalid canvases no-op; out-of-region coordinates clip; a null
 * plane simply drops that dimension (char-only, style-only, colour-only). No
 * allocation, no logging on any draw. Region geometry is borrowed for the call.
 * ======================================================================== */

TerminalPen TerminalPen_0(void) {
    return (TerminalPen) { TERMINAL_STYLE_NONE, TERMINAL_COLOR_DEFAULT, TERMINAL_COLOR_DEFAULT };
}
TerminalPen TerminalPen_1(uint8_t style) {
    return (TerminalPen) { style, TERMINAL_COLOR_DEFAULT, TERMINAL_COLOR_DEFAULT };
}
TerminalPen TerminalPen_2(uint8_t style, uint32_t fg) {
    return (TerminalPen) { style, fg, TERMINAL_COLOR_DEFAULT };
}
TerminalPen TerminalPen_3(uint8_t style, uint32_t fg, uint32_t bg) {
    return (TerminalPen) { style, fg, bg };
}

TerminalCanvas TerminalCanvas_0(void) {
    return (TerminalCanvas) {0};
}

static TerminalCanvas canvasMake(char *cells, uint8_t *styles, uint32_t *fg, uint32_t *bg,
                                 size_t width, size_t height) {
    TerminalCanvas c = {0};
    if (cells == nullptr || width == 0 || height == 0)
        return c;
    c.cells = cells;
    c.styles = styles;
    c.fg = fg;
    c.bg = bg;
    c.gridWidth = width;
    c.gridHeight = height;
    c.x = 0;
    c.y = 0;
    c.width = width;
    c.height = height;
    return c;
}

TerminalCanvas TerminalCanvas_4(char *cells, uint8_t *styles, size_t width, size_t height) {
    return canvasMake(cells, styles, nullptr, nullptr, width, height);
}

TerminalCanvas TerminalCanvas_6(char *cells, uint8_t *styles, uint32_t *fg, uint32_t *bg,
                                size_t width, size_t height) {
    return canvasMake(cells, styles, fg, bg, width, height);
}

bool TerminalCanvas_isValid(const TerminalCanvas *self) {
    return self != nullptr && (*self).cells != nullptr && (*self).width > 0 && (*self).height > 0;
}

static size_t minSize(size_t a, size_t b) { return a < b ? a : b; }

TerminalCanvas TerminalCanvas_sub(const TerminalCanvas *self, size_t lx, size_t ly,
                                  size_t w, size_t h) {
    TerminalCanvas out = {0};
    if (!TerminalCanvas_isValid(self))
        return out;
    if (lx >= (*self).width || ly >= (*self).height)
        return out;
    size_t ax = (*self).x + lx;
    size_t ay = (*self).y + ly;
    if (ax >= (*self).gridWidth || ay >= (*self).gridHeight)
        return out;
    size_t cw = minSize(w, (*self).width - lx);
    size_t ch = minSize(h, (*self).height - ly);
    cw = minSize(cw, (*self).gridWidth - ax);
    ch = minSize(ch, (*self).gridHeight - ay);
    if (cw == 0 || ch == 0)
        return out;
    out.cells = (*self).cells;
    out.styles = (*self).styles;
    out.fg = (*self).fg;
    out.bg = (*self).bg;
    out.gridWidth = (*self).gridWidth;
    out.gridHeight = (*self).gridHeight;
    out.x = ax;
    out.y = ay;
    out.width = cw;
    out.height = ch;
    return out;
}

static void putCell(const TerminalCanvas *self, size_t ax, size_t ay, char ch, TerminalPen pen) {
    if (ax >= (*self).gridWidth || ay >= (*self).gridHeight)
        return;
    size_t index = ay * (*self).gridWidth + ax;
    (*self).cells[index] = ch;
    if ((*self).styles != nullptr)
        (*self).styles[index] = pen.style;
    if ((*self).fg != nullptr)
        (*self).fg[index] = pen.fg;
    if ((*self).bg != nullptr)
        (*self).bg[index] = pen.bg;
}

void TerminalCanvas_fill(TerminalCanvas *self, char ch, TerminalPen pen) {
    if (!TerminalCanvas_isValid(self))
        return;
    for (size_t ly = 0; ly < (*self).height; ++ly)
        for (size_t lx = 0; lx < (*self).width; ++lx)
            putCell(self, (*self).x + lx, (*self).y + ly, ch, pen);
}

void TerminalCanvas_text(TerminalCanvas *self, size_t lx, size_t ly,
                         const char *text, size_t length, TerminalPen pen) {
    if (!TerminalCanvas_isValid(self) || text == nullptr || ly >= (*self).height)
        return;
    for (size_t i = 0; i < length; ++i) {
        if (lx + i >= (*self).width)
            break;
        unsigned char ch = (unsigned char) text[i];
        if (ch < 32 || ch >= 127)
            ch = '?';
        putCell(self, (*self).x + lx + i, (*self).y + ly, (char) ch, pen);
    }
}

void TerminalCanvas_hline(TerminalCanvas *self, size_t ly, char ch, TerminalPen pen) {
    if (!TerminalCanvas_isValid(self) || ly >= (*self).height)
        return;
    for (size_t lx = 0; lx < (*self).width; ++lx)
        putCell(self, (*self).x + lx, (*self).y + ly, ch, pen);
}

void TerminalCanvas_vline(TerminalCanvas *self, size_t lx, char ch, TerminalPen pen) {
    if (!TerminalCanvas_isValid(self) || lx >= (*self).width)
        return;
    for (size_t ly = 0; ly < (*self).height; ++ly)
        putCell(self, (*self).x + lx, (*self).y + ly, ch, pen);
}

void TerminalCanvas_box(TerminalCanvas *self, TerminalPen pen) {
    if (!TerminalCanvas_isValid(self))
        return;
    TerminalCanvas_hline(self, 0, '-', pen);
    TerminalCanvas_hline(self, (*self).height - 1, '-', pen);
    TerminalCanvas_vline(self, 0, '|', pen);
    TerminalCanvas_vline(self, (*self).width - 1, '|', pen);
    putCell(self, (*self).x, (*self).y, '+', pen);
    putCell(self, (*self).x + (*self).width - 1, (*self).y, '+', pen);
    putCell(self, (*self).x, (*self).y + (*self).height - 1, '+', pen);
    putCell(self, (*self).x + (*self).width - 1, (*self).y + (*self).height - 1, '+', pen);
}

char TerminalCanvas_charAt(const TerminalCanvas *self, size_t lx, size_t ly) {
    if (!TerminalCanvas_isValid(self) || lx >= (*self).width || ly >= (*self).height)
        return '\0';
    return (*self).cells[((*self).y + ly) * (*self).gridWidth + ((*self).x + lx)];
}

uint8_t TerminalCanvas_styleAt(const TerminalCanvas *self, size_t lx, size_t ly) {
    if (!TerminalCanvas_isValid(self) || (*self).styles == nullptr
        || lx >= (*self).width || ly >= (*self).height)
        return TERMINAL_STYLE_NONE;
    return (*self).styles[((*self).y + ly) * (*self).gridWidth + ((*self).x + lx)];
}

uint32_t TerminalCanvas_fgAt(const TerminalCanvas *self, size_t lx, size_t ly) {
    if (!TerminalCanvas_isValid(self) || (*self).fg == nullptr
        || lx >= (*self).width || ly >= (*self).height)
        return TERMINAL_COLOR_DEFAULT;
    return (*self).fg[((*self).y + ly) * (*self).gridWidth + ((*self).x + lx)];
}

uint32_t TerminalCanvas_bgAt(const TerminalCanvas *self, size_t lx, size_t ly) {
    if (!TerminalCanvas_isValid(self) || (*self).bg == nullptr
        || lx >= (*self).width || ly >= (*self).height)
        return TERMINAL_COLOR_DEFAULT;
    return (*self).bg[((*self).y + ly) * (*self).gridWidth + ((*self).x + lx)];
}

size_t TerminalCanvas_getWidth(const TerminalCanvas *self) { return self ? (*self).width : 0; }
size_t TerminalCanvas_getHeight(const TerminalCanvas *self) { return self ? (*self).height : 0; }
bool TerminalCanvas_hasStylePlane(const TerminalCanvas *self) { return self && (*self).styles != nullptr; }
bool TerminalCanvas_hasColorPlane(const TerminalCanvas *self) { return self && (*self).fg != nullptr; }

bool TerminalCanvas_copyRow(const TerminalCanvas *self, size_t ly, char *dest, size_t cap,
                            bool *outTruncated) {
    if (outTruncated != nullptr)
        *outTruncated = false;
    if (dest == nullptr || cap == 0) {
        if (outTruncated != nullptr)
            *outTruncated = true;
        THROW("TerminalCanvas copyRow: missing destination");
        return false;
    }
    if (!TerminalCanvas_isValid(self) || ly >= (*self).height) {
        dest[0] = '\0';
        return true;
    }
    size_t n = minSize((*self).width, cap - 1);
    for (size_t lx = 0; lx < n; ++lx)
        dest[lx] = TerminalCanvas_charAt(self, lx, ly);
    dest[n] = '\0';
    bool cut = n < (*self).width;
    if (outTruncated != nullptr)
        *outTruncated = cut;
    return !cut;
}

bool TerminalCanvas_toString(const TerminalCanvas *self, char *dest, size_t cap, bool *outTruncated) {
    if (outTruncated != nullptr)
        *outTruncated = false;
    if (dest == nullptr || cap == 0) {
        if (outTruncated != nullptr)
            *outTruncated = true;
        THROW("TerminalCanvas projection: missing destination");
        return false;
    }
    int n = self ? snprintf(dest, cap,
        "TerminalCanvas{region=%zux%zu@%zu,%zu,grid=%zux%zu,styles=%s,color=%s,valid=%d}",
        (*self).width, (*self).height, (*self).x, (*self).y,
        (*self).gridWidth, (*self).gridHeight,
        (*self).styles ? "yes" : "none", (*self).fg ? "yes" : "none",
        TerminalCanvas_isValid(self))
        : snprintf(dest, cap, "nullptr");
    bool cut = n < 0 || (size_t) n >= cap;
    if (outTruncated != nullptr)
        *outTruncated = cut;
    if (cut)
        THROW("TerminalCanvas projection: truncated at %zu bytes", cap);
    return !cut;
}

bool TerminalCanvas_toStringStruct(const TerminalCanvas *self, char *dest, size_t cap, bool *outTruncated) {
    return TerminalCanvas_toString(self, dest, cap, outTruncated);
}
