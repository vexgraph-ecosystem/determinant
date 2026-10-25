#include "ui/panel/terminal_frame.h"
#include "format/terminal_style.h"
#include "c23/constructor.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** A bare region layout. TerminalFrame splits a rectangle into header / left /
 * centre / right / footer canvases and draws only its own rules; every other
 * pixel belongs to the caller. It carries no notion of a chat, a list, a badge
 * or a setting — those are application content. Geometry is recomputed from the
 * root size on each call, so resize is free and rails collapse on a narrow
 * terminal to keep the centre usable. Owns no memory; may paint one borrowed
 * background image. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalFrame (ui/panel/terminal_frame.c)
 * STRUCT FIELDS (Mirroring ui/panel/terminal_frame.h)
 * size_t leftWidth, rightWidth   requested column widths
 * size_t headerRows, footerRows  top/bottom band rows
 * const TerminalImage *background  borrowed background image (nullable)
 * TerminalRule rule              connector glyph set (see TerminalRule)
 * PRIVATE HELPERS
 * clampRail(requested, total)  rail width clamped to a third of the root
 * frameGeometry(self, root)    computed rails + band offsets
 * FUNCTION REGISTRY (exports)
 * Constructors: TerminalFrame_0(void), TerminalFrame_2(left, right)
 * Rules: TerminalRule_0/_2/_4, TerminalRule_draw(rule, canvas, ly, pen),
 *   TerminalRule_drawCut(rule, canvas, ly, inset, pen)  cut, stopped-short run
 *   TerminalRule_drawBox(rule, canvas, pen)             detached box + corners
 *   TerminalFrame_setRule/getRule(self, rule)
 * Config: setRails/getRails, setBands/getBands, setBackground/getBackground
 * Layout: TerminalFrame_getLayout(self, root, outLeft, outRight, outRails)
 * Regions: TerminalFrame_header/left/center/right/footer(self, root)
 * Paint: TerminalFrame_paintBackground, TerminalFrame_drawRules(self, root, pen)
 * Projections: TerminalProjection TerminalFrame_toString/toStringStruct(self)
 * CONTRACT: a null/invalid root yields an invalid region (draws no-op); regions
 * are clipped to the root and to the grid; no allocation, no logging.
 * ======================================================================== */

typedef struct FrameGeometry {
    size_t left, right;      // effective column widths (0 => no rails)
    bool rails;
    size_t headerRows, bodyY, bodyH, footerRows;
} FrameGeometry;

static size_t clampRail(size_t requested, size_t total) {
    size_t third = total / 3;
    return requested < third ? requested : third;
}

static FrameGeometry frameGeometry(const TerminalFrame *self, const TerminalCanvas *root) {
    FrameGeometry g = {0};
    size_t W = TerminalCanvas_getWidth(root);
    size_t H = TerminalCanvas_getHeight(root);
    g.left = clampRail((*self).leftWidth, W);
    g.right = clampRail((*self).rightWidth, W);
    if (g.left + g.right + 3 > W) {
        g.left = 0;
        g.right = 0;
    }
    g.rails = g.left > 0 && g.right > 0;
    g.headerRows = (*self).headerRows;
    g.footerRows = (*self).footerRows;
    if (g.headerRows + g.footerRows >= H) {
        g.headerRows = 0;
        g.footerRows = 0;
    }
    g.bodyY = g.headerRows;
    g.bodyH = H - g.headerRows - g.footerRows;
    return g;
}

static size_t centreX(const FrameGeometry *g) { return g->rails ? g->left + 1 : 0; }
static size_t centreW(const FrameGeometry *g, const TerminalCanvas *root) {
    size_t W = TerminalCanvas_getWidth(root);
    return W - g->left - g->right - (g->rails ? 2 : 0);
}

TerminalFrame TerminalFrame_0(void) { return TerminalFrame_2(0, 0); }

TerminalFrame TerminalFrame_2(size_t leftWidth, size_t rightWidth) {
    TerminalFrame frame = {0};
    frame.leftWidth = leftWidth;
    frame.rightWidth = rightWidth;
    frame.headerRows = TERMINAL_FRAME_HEADER_ROWS_DEFAULT;
    frame.rule = TerminalRule_0();
    return frame;
}

TerminalRule TerminalRule_0(void) {
    return (TerminalRule) { '-', '|', '+', '+', 0, 0, 0, 0, 0, 0, 0, 0 };
}
TerminalRule TerminalRule_2(uint32_t horizontal, uint32_t vertical) {
    return (TerminalRule) { horizontal, vertical, '+', '+', 0, 0, 0, 0, 0, 0, 0, 0 };
}
TerminalRule TerminalRule_4(uint32_t horizontal, uint32_t vertical, uint32_t stripe, size_t stripeEvery) {
    return (TerminalRule) { horizontal, vertical, '+', '+', stripe, stripeEvery, 0, 0, 0, 0, 0, 0 };
}

void TerminalRule_draw(const TerminalRule *rule, TerminalCanvas *canvas, size_t ly, TerminalPen pen) {
    if (rule == nullptr || !TerminalCanvas_isValid(canvas))
        return;
    size_t width = TerminalCanvas_getWidth(canvas);
    for (size_t x = 0; x < width; ++x) {
        uint32_t ch = (*rule).horizontal;
        if ((*rule).stripe != 0 && (*rule).stripeEvery > 0 && (x % (*rule).stripeEvery) == ((*rule).stripeEvery - 1))
            ch = (*rule).stripe;
        TerminalCanvas_glyph(canvas, x, ly, ch, pen);
    }
}

// A CUT run: it stops short of both edges and ends in the cut-endpoint glyphs, so
// a panel divider reads as `╶──────╴` (or a plain, clearly-ended ` ------ `)
// rather than a line that runs into the borders.
void TerminalRule_drawCut(const TerminalRule *rule, TerminalCanvas *canvas, size_t ly,
                          size_t inset, TerminalPen pen) {
    if (rule == nullptr || !TerminalCanvas_isValid(canvas))
        return;
    size_t width = TerminalCanvas_getWidth(canvas);
    if (width <= inset * 2)
        return;
    size_t span = width - inset * 2;
    size_t start = inset;
    for (size_t i = 0; i < span; ++i) {
        uint32_t ch = (*rule).horizontal;
        if ((*rule).stripe != 0 && (*rule).stripeEvery > 0 && (i % (*rule).stripeEvery) == ((*rule).stripeEvery - 1))
            ch = (*rule).stripe;
        if (i == 0 && (*rule).cutLeft != 0)
            ch = (*rule).cutLeft;
        if (i == span - 1 && (*rule).cutRight != 0)
            ch = (*rule).cutRight;
        TerminalCanvas_glyph(canvas, start + i, ly, ch, pen);
    }
}

// A DETACHED box: its own four corners and its own verticals. Nothing joins it to
// a neighbour, which is what keeps panels reading as separate instruments.
void TerminalRule_drawBox(const TerminalRule *rule, TerminalCanvas *canvas, TerminalPen pen) {
    if (rule == nullptr || !TerminalCanvas_isValid(canvas))
        return;
    size_t width = TerminalCanvas_getWidth(canvas);
    size_t height = TerminalCanvas_getHeight(canvas);
    if (width < 2 || height < 2)
        return;
    uint32_t horizontal = (*rule).horizontal;
    uint32_t vertical = (*rule).vertical;
    uint32_t topLeft = (*rule).cornerTopLeft ? (*rule).cornerTopLeft : (*rule).corner;
    uint32_t topRight = (*rule).cornerTopRight ? (*rule).cornerTopRight : (*rule).corner;
    uint32_t bottomLeft = (*rule).cornerBottomLeft ? (*rule).cornerBottomLeft : (*rule).corner;
    uint32_t bottomRight = (*rule).cornerBottomRight ? (*rule).cornerBottomRight : (*rule).corner;
    TerminalCanvas_hline(canvas, 0, horizontal, pen);
    TerminalCanvas_hline(canvas, height - 1, horizontal, pen);
    TerminalCanvas_vline(canvas, 0, vertical, pen);
    TerminalCanvas_vline(canvas, width - 1, vertical, pen);
    TerminalCanvas_glyph(canvas, 0, 0, topLeft, pen);
    TerminalCanvas_glyph(canvas, width - 1, 0, topRight, pen);
    TerminalCanvas_glyph(canvas, 0, height - 1, bottomLeft, pen);
    TerminalCanvas_glyph(canvas, width - 1, height - 1, bottomRight, pen);
}

bool TerminalFrame_setRule(TerminalFrame *self, TerminalRule rule) {
    if (self == nullptr) { THROW("TerminalFrame rule: null self"); return false; }
    (*self).rule = rule;
    return true;
}
TerminalRule TerminalFrame_getRule(const TerminalFrame *self) {
    return self ? (*self).rule : TerminalRule_0();
}

bool TerminalFrame_setRails(TerminalFrame *self, size_t leftWidth, size_t rightWidth) {
    if (self == nullptr) {
        THROW("TerminalFrame rails: null self");
        return false;
    }
    (*self).leftWidth = leftWidth;
    (*self).rightWidth = rightWidth;
    return true;
}
TerminalFramePair TerminalFrame_getRails(const TerminalFrame *self) {
    return self ? (TerminalFramePair) { (*self).leftWidth, (*self).rightWidth } : (TerminalFramePair) { 0u, 0u };
}
bool TerminalFrame_setBands(TerminalFrame *self, size_t headerRows, size_t footerRows) {
    if (self == nullptr) {
        THROW("TerminalFrame bands: null self");
        return false;
    }
    (*self).headerRows = headerRows;
    (*self).footerRows = footerRows;
    return true;
}
TerminalFramePair TerminalFrame_getBands(const TerminalFrame *self) {
    return self ? (TerminalFramePair) { (*self).headerRows, (*self).footerRows } : (TerminalFramePair) { 0u, 0u };
}
bool TerminalFrame_setBackground(TerminalFrame *self, const TerminalImage *image) {
    if (self == nullptr) {
        THROW("TerminalFrame background: null self");
        return false;
    }
    (*self).background = image;
    return true;
}
const TerminalImage *TerminalFrame_getBackground(const TerminalFrame *self) {
    return self ? (*self).background : nullptr;
}

TerminalFrameLayout TerminalFrame_getLayout(const TerminalFrame *self, const TerminalCanvas *root) {
    FrameGeometry g = {0};
    if (self != nullptr && TerminalCanvas_isValid(root))
        g = frameGeometry(self, root);
    return (TerminalFrameLayout) { g.left, g.right, g.rails };
}

void TerminalFrame_paintBackground(const TerminalFrame *self, TerminalCanvas *root) {
    if (self == nullptr || (*self).background == nullptr || !TerminalCanvas_isValid(root))
        return;
    TerminalImage_renderFit((*self).background, root);
}

TerminalCanvas TerminalFrame_header(const TerminalFrame *self, const TerminalCanvas *root) {
    if (self == nullptr || !TerminalCanvas_isValid(root))
        return TerminalCanvas_0();
    FrameGeometry g = frameGeometry(self, root);
    if (g.headerRows == 0)
        return TerminalCanvas_0();
    return TerminalCanvas_sub(root, 0, 0, TerminalCanvas_getWidth(root), g.headerRows);
}

TerminalCanvas TerminalFrame_left(const TerminalFrame *self, const TerminalCanvas *root) {
    if (self == nullptr || !TerminalCanvas_isValid(root))
        return TerminalCanvas_0();
    FrameGeometry g = frameGeometry(self, root);
    if (!g.rails)
        return TerminalCanvas_0();
    return TerminalCanvas_sub(root, 0, g.bodyY, g.left, g.bodyH);
}

TerminalCanvas TerminalFrame_center(const TerminalFrame *self, const TerminalCanvas *root) {
    if (self == nullptr || !TerminalCanvas_isValid(root))
        return TerminalCanvas_0();
    FrameGeometry g = frameGeometry(self, root);
    size_t w = centreW(&g, root);
    if (w == 0 || g.bodyH == 0)
        return TerminalCanvas_0();
    return TerminalCanvas_sub(root, centreX(&g), g.bodyY, w, g.bodyH);
}

TerminalCanvas TerminalFrame_right(const TerminalFrame *self, const TerminalCanvas *root) {
    if (self == nullptr || !TerminalCanvas_isValid(root))
        return TerminalCanvas_0();
    FrameGeometry g = frameGeometry(self, root);
    if (!g.rails)
        return TerminalCanvas_0();
    return TerminalCanvas_sub(root, TerminalCanvas_getWidth(root) - g.right, g.bodyY, g.right, g.bodyH);
}

TerminalCanvas TerminalFrame_footer(const TerminalFrame *self, const TerminalCanvas *root) {
    if (self == nullptr || !TerminalCanvas_isValid(root))
        return TerminalCanvas_0();
    FrameGeometry g = frameGeometry(self, root);
    size_t w = centreW(&g, root);
    if (g.footerRows == 0 || w == 0)
        return TerminalCanvas_0();
    size_t y = TerminalCanvas_getHeight(root) - g.footerRows;
    return TerminalCanvas_sub(root, centreX(&g), y, w, g.footerRows);
}

void TerminalFrame_drawRules(const TerminalFrame *self, TerminalCanvas *root, TerminalPen pen) {
    if (self == nullptr || !TerminalCanvas_isValid(root))
        return;
    FrameGeometry g = frameGeometry(self, root);
    const TerminalRule *rule = &(*self).rule;
    size_t W = TerminalCanvas_getWidth(root);
    if (g.headerRows > 0) {
        TerminalCanvas header = TerminalCanvas_sub(root, 0, 0, W, g.headerRows);
        TerminalRule_draw(rule, &header, g.headerRows - 1, pen);
        if (g.rails) {
            // Join the run to each rail with a junction glyph (the connector).
            uint32_t j = (*rule).junction;
            TerminalCanvas_glyph(root, g.left, g.headerRows - 1, j, pen);
            TerminalCanvas_glyph(root, W - g.right - 1, g.headerRows - 1, j, pen);
        }
    }
    if (g.rails) {
        TerminalCanvas leftRule = TerminalCanvas_sub(root, g.left, g.bodyY, 1, g.bodyH);
        TerminalCanvas_vline(&leftRule, 0, (*rule).vertical, pen);
        TerminalCanvas rightRule = TerminalCanvas_sub(root, W - g.right - 1, g.bodyY, 1, g.bodyH);
        TerminalCanvas_vline(&rightRule, 0, (*rule).vertical, pen);
        if (g.headerRows == 0) {
            // No header run: cap each rail with the corner glyph.
            uint32_t c = (*rule).corner;
            TerminalCanvas_glyph(root, g.left, g.bodyY, c, pen);
            TerminalCanvas_glyph(root, W - g.right - 1, g.bodyY, c, pen);
        }
    }
}

TerminalProjection TerminalFrame_toString(const TerminalFrame *self) {
    return TerminalFrame_toStringStruct(self);
}

TerminalProjection TerminalFrame_toStringStruct(const TerminalFrame *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalFrame{left=%zu,right=%zu,header=%zu,footer=%zu,background=%d}",
        (*self).leftWidth, (*self).rightWidth, (*self).headerRows, (*self).footerRows,
        (*self).background != nullptr);
}
