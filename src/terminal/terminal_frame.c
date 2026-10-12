#include "terminal/terminal_frame.h"
#include "terminal/terminal_style.h"
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
 * CLASS: TerminalFrame (terminal/terminal_frame.c)
 * STRUCT FIELDS (Mirroring terminal/terminal_frame.h)
 * size_t leftWidth, rightWidth   requested column widths
 * size_t headerRows, footerRows  top/bottom band rows
 * const TerminalImage *background  borrowed background image (nullable)
 * PRIVATE HELPERS
 * clampRail(requested, total)  rail width clamped to a third of the root
 * geometry(self, root, out)    computed rails + band offsets
 * FUNCTION REGISTRY (exports)
 * Constructors: TerminalFrame_0(void), TerminalFrame_2(left, right)
 * Config: setRails/getRails, setBands/getBands, setBackground/getBackground
 * Layout: TerminalFrame_getLayout(self, root, outLeft, outRight, outRails)
 * Regions: TerminalFrame_header/left/center/right/footer(self, root)
 * Paint: TerminalFrame_paintBackground, TerminalFrame_drawRules(self, root, pen)
 * Projections: bool TerminalFrame_toString/toStringStruct(...)
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
    return frame;
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
void TerminalFrame_getRails(const TerminalFrame *self, size_t *outLeft, size_t *outRight) {
    if (outLeft) *outLeft = self ? (*self).leftWidth : 0;
    if (outRight) *outRight = self ? (*self).rightWidth : 0;
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
void TerminalFrame_getBands(const TerminalFrame *self, size_t *outHeader, size_t *outFooter) {
    if (outHeader) *outHeader = self ? (*self).headerRows : 0;
    if (outFooter) *outFooter = self ? (*self).footerRows : 0;
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

void TerminalFrame_getLayout(const TerminalFrame *self, const TerminalCanvas *root,
                             size_t *outLeft, size_t *outRight, bool *outRails) {
    FrameGeometry g = {0};
    if (self != nullptr && TerminalCanvas_isValid(root))
        g = frameGeometry(self, root);
    if (outLeft) *outLeft = g.left;
    if (outRight) *outRight = g.right;
    if (outRails) *outRails = g.rails;
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

void TerminalFrame_drawRules(const TerminalFrame *self, TerminalCanvas *root, TerminalPen rule) {
    if (self == nullptr || !TerminalCanvas_isValid(root))
        return;
    FrameGeometry g = frameGeometry(self, root);
    if (g.headerRows > 0) {
        TerminalCanvas header = TerminalCanvas_sub(root, 0, 0, TerminalCanvas_getWidth(root), g.headerRows);
        TerminalCanvas_hline(&header, g.headerRows - 1, '-', rule);
    }
    if (g.rails) {
        TerminalCanvas leftRule = TerminalCanvas_sub(root, g.left, g.bodyY, 1, g.bodyH);
        TerminalCanvas_vline(&leftRule, 0, '|', rule);
        TerminalCanvas rightRule = TerminalCanvas_sub(root, TerminalCanvas_getWidth(root) - g.right - 1, g.bodyY, 1, g.bodyH);
        TerminalCanvas_vline(&rightRule, 0, '|', rule);
    }
}

bool TerminalFrame_toString(const TerminalFrame *self, char *dest, size_t cap, bool *outTruncated) {
    if (outTruncated != nullptr)
        *outTruncated = false;
    if (dest == nullptr || cap == 0) {
        if (outTruncated != nullptr)
            *outTruncated = true;
        THROW("TerminalFrame projection: missing destination");
        return false;
    }
    int n = self ? snprintf(dest, cap,
        "TerminalFrame{left=%zu,right=%zu,header=%zu,footer=%zu,background=%d}",
        (*self).leftWidth, (*self).rightWidth, (*self).headerRows, (*self).footerRows,
        (*self).background != nullptr)
        : snprintf(dest, cap, "nullptr");
    bool cut = n < 0 || (size_t) n >= cap;
    if (outTruncated != nullptr)
        *outTruncated = cut;
    if (cut)
        THROW("TerminalFrame projection: truncated at %zu bytes", cap);
    return !cut;
}

bool TerminalFrame_toStringStruct(const TerminalFrame *self, char *dest, size_t cap, bool *outTruncated) {
    return TerminalFrame_toString(self, dest, cap, outTruncated);
}
