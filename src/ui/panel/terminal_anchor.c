#include "ui/panel/terminal_anchor.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <stdio.h>

;;DEFINITION
/** The placement vocabulary, and the single owner of band math. An anchor answers
 * one question — given the parent's running cursor, where does this region land,
 * and what cursor is left for the next one? Answering it as a PURE function is
 * what makes a whole panel predictable: a title band, a status band, two rails
 * and a centre compose in insertion order with no layout pass, no dirty flag and
 * no second geometry ruleset, because each anchor simply consumes its slice and
 * hands the remainder forward. Bands reserve; fills and pins do not; that single
 * distinction is why an overlay can float above a flow that never notices it.
 * Everything is clamped rather than wrapped, so a cursor smaller than a request
 * yields an empty region instead of an underflowed rectangle. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalAnchor (ui/panel/terminal_anchor.c)
 * STRUCT FIELDS (Mirroring ui/panel/terminal_anchor.h)
 * TerminalAnchorKind kind        placement family (fill, band, pin)
 * size_t extent                  band thickness in cells (AUTO = whole cursor)
 * size_t width                   pin width in cells (AUTO = cursor width)
 * size_t height                  pin height in cells (AUTO = cursor height)
 * size_t insetX                  inset applied inside the placed region
 * size_t insetY
 * SLOT RECORD: TerminalRect
 * size_t x, y, width, height     a cell region in the parent's space
 * SLOT RECORD: TerminalAnchorPlacement
 * TerminalRect region            where the layer landed
 * TerminalRect cursor            the cursor left for the next layer
 * PRIVATE HELPERS
 * minSize(a,b)               the smaller of two extents
 * bandExtent(extent, avail)  a band request narrowed to what the cursor holds
 * pinStartX/pinStartY(kind,…)  the nine-point axis rule
 * applyInset(region, x, y)   symmetric inset, collapsed rather than underflowed
 * FUNCTION REGISTRY (exports)
 * Constructors: TerminalAnchor_0(void) [FILL],
 *   _1(kind), _2(kind, extent) for bands, _3(kind, width, height) for pins
 * Queries: bool isBand/reserves(self)
 *   TerminalAnchorKind getKind(self)
 *   size_t getExtent/getWidth/getHeight/getInsetX/getInsetY(self)
 * Configuration: bool setKind/setExtent/setSize/setInset(self, …)
 * Placement: TerminalAnchorPlacement TerminalAnchor_place(self, cursor)
 * Projections: TerminalProjection TerminalAnchor_toString/toStringStruct(self)
 * CONTRACT: place is pure and total — a null anchor returns the cursor unchanged
 * as the region, an over-large extent or inset clamps to an empty region, and no
 * input can wrap a size_t. A null self on a setter is rejected loudly and changes
 * nothing; getters answer safe defaults. No allocation, no logging on any path.
 * ======================================================================== */

static size_t minSize(size_t a, size_t b) { return a < b ? a : b; }

static bool validKind(TerminalAnchorKind kind) {
    return kind >= TERMINAL_ANCHOR_FILL && kind <= TERMINAL_ANCHOR_PIN_BOTTOM_RIGHT;
}

// A band asks for `extent` cells; the cursor offers `available`. AUTO means take
// the lot. The result is never larger than the cursor.
static size_t bandExtent(size_t extent, size_t available) {
    if (extent == TERMINAL_ANCHOR_AUTO)
        return available;
    return minSize(extent, available);
}

// The nine-point pin rule, one axis at a time. `offset` is the room left over
// once the pin's own size is clamped into the cursor, so it never underflows.
static size_t pinStartX(TerminalAnchorKind kind, size_t cursorX, size_t offset) {
    switch (kind) {
        case TERMINAL_ANCHOR_PIN_TOP_RIGHT:
        case TERMINAL_ANCHOR_PIN_CENTER_RIGHT:
        case TERMINAL_ANCHOR_PIN_BOTTOM_RIGHT:
            return cursorX + offset;
        case TERMINAL_ANCHOR_PIN_TOP_CENTER:
        case TERMINAL_ANCHOR_PIN_CENTER:
        case TERMINAL_ANCHOR_PIN_BOTTOM_CENTER:
            return cursorX + offset / 2;
        default:
            return cursorX;
    }
}

static size_t pinStartY(TerminalAnchorKind kind, size_t cursorY, size_t offset) {
    switch (kind) {
        case TERMINAL_ANCHOR_PIN_BOTTOM_LEFT:
        case TERMINAL_ANCHOR_PIN_BOTTOM_CENTER:
        case TERMINAL_ANCHOR_PIN_BOTTOM_RIGHT:
            return cursorY + offset;
        case TERMINAL_ANCHOR_PIN_CENTER_LEFT:
        case TERMINAL_ANCHOR_PIN_CENTER:
        case TERMINAL_ANCHOR_PIN_CENTER_RIGHT:
            return cursorY + offset / 2;
        default:
            return cursorY;
    }
}

// A symmetric inset inside the placed region. A cut deeper than half the region
// empties it instead of wrapping, so the result is always a valid rectangle.
static TerminalRect applyInset(TerminalRect region, size_t insetX, size_t insetY) {
    size_t cutX = insetX < region.width / 2 ? insetX : region.width / 2;
    size_t cutY = insetY < region.height / 2 ? insetY : region.height / 2;
    region.x += cutX;
    region.y += cutY;
    region.width -= cutX * 2;
    region.height -= cutY * 2;
    return region;
}

TerminalAnchor TerminalAnchor_0(void) {
    return (TerminalAnchor) { TERMINAL_ANCHOR_FILL, TERMINAL_ANCHOR_AUTO, TERMINAL_ANCHOR_AUTO,
                              TERMINAL_ANCHOR_AUTO, 0, 0 };
}

TerminalAnchor TerminalAnchor_1(TerminalAnchorKind kind) {
    TerminalAnchor self = TerminalAnchor_0();
    if (!TerminalAnchor_setKind(&self, kind))
        return TerminalAnchor_0();
    return self;
}

TerminalAnchor TerminalAnchor_2(TerminalAnchorKind kind, size_t extent) {
    TerminalAnchor self = TerminalAnchor_1(kind);
    self.extent = extent;
    return self;
}

TerminalAnchor TerminalAnchor_3(TerminalAnchorKind kind, size_t width, size_t height) {
    TerminalAnchor self = TerminalAnchor_1(kind);
    self.width = width;
    self.height = height;
    return self;
}

bool TerminalAnchor_isBand(const TerminalAnchor *self) {
    if (self == nullptr)
        return false;
    switch ((*self).kind) {
        case TERMINAL_ANCHOR_BAND_TOP:
        case TERMINAL_ANCHOR_BAND_BOTTOM:
        case TERMINAL_ANCHOR_BAND_LEFT:
        case TERMINAL_ANCHOR_BAND_RIGHT:
            return true;
        default:
            return false;
    }
}

bool TerminalAnchor_reserves(const TerminalAnchor *self) { return TerminalAnchor_isBand(self); }

TerminalAnchorKind TerminalAnchor_getKind(const TerminalAnchor *self) {
    return self ? (*self).kind : TERMINAL_ANCHOR_FILL;
}
size_t TerminalAnchor_getExtent(const TerminalAnchor *self) {
    return self ? (*self).extent : TERMINAL_ANCHOR_AUTO;
}
size_t TerminalAnchor_getWidth(const TerminalAnchor *self) {
    return self ? (*self).width : TERMINAL_ANCHOR_AUTO;
}
size_t TerminalAnchor_getHeight(const TerminalAnchor *self) {
    return self ? (*self).height : TERMINAL_ANCHOR_AUTO;
}
size_t TerminalAnchor_getInsetX(const TerminalAnchor *self) { return self ? (*self).insetX : 0; }
size_t TerminalAnchor_getInsetY(const TerminalAnchor *self) { return self ? (*self).insetY : 0; }

bool TerminalAnchor_setKind(TerminalAnchor *self, TerminalAnchorKind kind) {
    if (self == nullptr || !validKind(kind)) {
        THROW("TerminalAnchor kind: null anchor or unknown kind");
        return false;
    }
    (*self).kind = kind;
    return true;
}

bool TerminalAnchor_setExtent(TerminalAnchor *self, size_t extent) {
    if (self == nullptr) {
        THROW("TerminalAnchor extent: null anchor");
        return false;
    }
    (*self).extent = extent;
    return true;
}

bool TerminalAnchor_setSize(TerminalAnchor *self, size_t width, size_t height) {
    if (self == nullptr) {
        THROW("TerminalAnchor size: null anchor");
        return false;
    }
    (*self).width = width;
    (*self).height = height;
    return true;
}

bool TerminalAnchor_setInset(TerminalAnchor *self, size_t insetX, size_t insetY) {
    if (self == nullptr) {
        THROW("TerminalAnchor inset: null anchor");
        return false;
    }
    (*self).insetX = insetX;
    (*self).insetY = insetY;
    return true;
}

TerminalAnchorPlacement TerminalAnchor_place(const TerminalAnchor *self, TerminalRect cursor) {
    TerminalAnchorPlacement out = { .region = cursor, .cursor = cursor };
    if (self == nullptr)
        return out;
    TerminalRect region = cursor;
    switch ((*self).kind) {
        case TERMINAL_ANCHOR_BAND_TOP: {
            size_t take = bandExtent((*self).extent, cursor.height);
            region = (TerminalRect) { cursor.x, cursor.y, cursor.width, take };
            out.cursor.y = cursor.y + take;
            out.cursor.height = cursor.height - take;
            break;
        }
        case TERMINAL_ANCHOR_BAND_BOTTOM: {
            size_t take = bandExtent((*self).extent, cursor.height);
            region = (TerminalRect) { cursor.x, cursor.y + (cursor.height - take), cursor.width, take };
            out.cursor.height = cursor.height - take;
            break;
        }
        case TERMINAL_ANCHOR_BAND_LEFT: {
            size_t take = bandExtent((*self).extent, cursor.width);
            region = (TerminalRect) { cursor.x, cursor.y, take, cursor.height };
            out.cursor.x = cursor.x + take;
            out.cursor.width = cursor.width - take;
            break;
        }
        case TERMINAL_ANCHOR_BAND_RIGHT: {
            size_t take = bandExtent((*self).extent, cursor.width);
            region = (TerminalRect) { cursor.x + (cursor.width - take), cursor.y, take, cursor.height };
            out.cursor.width = cursor.width - take;
            break;
        }
        case TERMINAL_ANCHOR_FILL:
            break;
        default: {
            size_t width = (*self).width == TERMINAL_ANCHOR_AUTO
                ? cursor.width : minSize((*self).width, cursor.width);
            size_t height = (*self).height == TERMINAL_ANCHOR_AUTO
                ? cursor.height : minSize((*self).height, cursor.height);
            region = (TerminalRect) { pinStartX((*self).kind, cursor.x, cursor.width - width),
                                      pinStartY((*self).kind, cursor.y, cursor.height - height),
                                      width, height };
            break;
        }
    }
    out.region = applyInset(region, (*self).insetX, (*self).insetY);
    return out;
}

static const char *kindName(TerminalAnchorKind kind) {
    switch (kind) {
        case TERMINAL_ANCHOR_FILL: return "fill";
        case TERMINAL_ANCHOR_BAND_TOP: return "band-top";
        case TERMINAL_ANCHOR_BAND_BOTTOM: return "band-bottom";
        case TERMINAL_ANCHOR_BAND_LEFT: return "band-left";
        case TERMINAL_ANCHOR_BAND_RIGHT: return "band-right";
        case TERMINAL_ANCHOR_PIN_TOP_LEFT: return "pin-top-left";
        case TERMINAL_ANCHOR_PIN_TOP_CENTER: return "pin-top-center";
        case TERMINAL_ANCHOR_PIN_TOP_RIGHT: return "pin-top-right";
        case TERMINAL_ANCHOR_PIN_CENTER_LEFT: return "pin-center-left";
        case TERMINAL_ANCHOR_PIN_CENTER: return "pin-center";
        case TERMINAL_ANCHOR_PIN_CENTER_RIGHT: return "pin-center-right";
        case TERMINAL_ANCHOR_PIN_BOTTOM_LEFT: return "pin-bottom-left";
        case TERMINAL_ANCHOR_PIN_BOTTOM_CENTER: return "pin-bottom-center";
        case TERMINAL_ANCHOR_PIN_BOTTOM_RIGHT: return "pin-bottom-right";
        default: return "unknown";
    }
}

TerminalProjection TerminalAnchor_toString(const TerminalAnchor *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format("TerminalAnchor{%s,reserves=%d,extent=%zu}",
        kindName((*self).kind), TerminalAnchor_reserves(self), (*self).extent);
}

TerminalProjection TerminalAnchor_toStringStruct(const TerminalAnchor *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalAnchor{kind=%s,extent=%zu,width=%zu,height=%zu,insetX=%zu,insetY=%zu}",
        kindName((*self).kind), (*self).extent, (*self).width, (*self).height,
        (*self).insetX, (*self).insetY);
}
