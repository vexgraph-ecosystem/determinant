#include "ui/text/terminal_text.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** The addressable leaf. A terminal interface is panels and text; this is the
 * text, and it is an object rather than a string handed to a draw call because
 * the interface must be able to find it again — to recolour it on hover, to ask
 * how wide it is, to move it. Identity is what makes hover possible, so the leaf
 * carries its own record even though its glyphs stay borrowed.
 *
 * It draws on one row of whatever region its parent resolved, with its own pen,
 * optional bracket glyphs and an alignment. Colours it does not set are inherited
 * from the parent pen, so a panel can declare "everything on me is grey" once and
 * every text on it agrees; styles combine instead, because a bitmask composes.
 * Nothing here allocates per paint or per cell, and a text never holds children. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalText (ui/text/terminal_text.c)
 * STRUCT FIELDS (Mirroring ui/text/terminal_text.h)
 * const char *glyphs            borrowed UTF-8, must outlive the text
 * size_t length                 bytes in `glyphs`
 * TerminalPen pen               this text's own style/foreground/background
 * uint32_t bracketLeft          codepoint before the text (0 = none)
 * uint32_t bracketRight         codepoint after the text (0 = none)
 * TerminalTextAlign align       placement inside the resolved region
 * PRIVATE HELPERS
 * validGlyphs(glyphs, length)  borrowed string admission (non-null when length != 0)
 * resolvePen(own, inherited)   inherit unset colours, combine styles
 * leadingCells(body, width)    the alignment offset for a measured body
 * FUNCTION REGISTRY (exports)
 * Constructors: TerminalText_0(void), _1(glyphs), _2(glyphs, length)
 * Lifecycle: void TerminalText_free(self)
 * Configuration: bool setText/getText/getLength, setPen/getPen,
 *   setStyle/getStyle, setForegroundColor/getForegroundColor,
 *   setBackgroundColor/getBackgroundColor, setBrackets/getBracketLeft/getBracketRight,
 *   setAlign/getAlign
 * Measure: size_t TerminalText_measure(self)
 * Paint: void TerminalText_paint(self, region, parentPen)
 * Projections: TerminalProjection TerminalText_toString/toStringStruct(self)
 * CONTRACT: a null text, a null glyph span carrying a non-zero length, or an
 * unknown alignment is rejected loudly and changes nothing; getters answer safe
 * defaults. The glyph string is borrowed and never freed here. Paint is clipped
 * to the region, quiet, and allocation-free; a text wider than its region is cut
 * at the region edge, so an over-wide label degrades rather than wrapping.
 * ======================================================================== */

static bool validGlyphs(const char *glyphs, size_t length) {
    return glyphs != nullptr || length == 0;
}

static bool validAlign(TerminalTextAlign align) {
    return align >= TERMINAL_TEXT_ALIGN_LEFT && align <= TERMINAL_TEXT_ALIGN_RIGHT;
}

// A colour the text did not set is taken from the parent; styles combine, since
// they are a bitmask and a parent DIM plus a child BOLD is a meaningful request.
static TerminalPen resolvePen(const TerminalPen *own, TerminalPen inherited) {
    TerminalPen pen = *own;
    if (!TERMINAL_COLOR_IS_SET(pen.fg))
        pen.fg = inherited.fg;
    if (!TERMINAL_COLOR_IS_SET(pen.bg))
        pen.bg = inherited.bg;
    pen.style |= inherited.style;
    return pen;
}

TerminalText *TerminalText_0(void) {
    TerminalText *self = Memory_alloc(TYPE_BYTE_ARRAY, sizeof(TerminalText));
    if (self == nullptr)
        return nullptr; // R2 owner reports/counts allocation exhaustion.
    *self = (TerminalText) { .glyphs = "", .length = 0, .pen = TerminalPen_0(),
                             .align = TERMINAL_TEXT_ALIGN_LEFT };
    return self;
}

TerminalText *TerminalText_1(const char *glyphs) {
    return TerminalText(glyphs, glyphs != nullptr ? strlen(glyphs) : 0);
}

TerminalText *TerminalText_2(const char *glyphs, size_t length) {
    if (!validGlyphs(glyphs, length)) {
        THROW("TerminalText construct: null glyph span with a non-zero length");
        return nullptr;
    }
    TerminalText *self = TerminalText_0();
    if (self == nullptr)
        return nullptr;
    (*self).glyphs = length == 0 ? "" : glyphs;
    (*self).length = length;
    return self;
}

void TerminalText_free(TerminalText *self) {
    if (self == nullptr)
        return;
    Memory_free(self);
}

bool TerminalText_setText(TerminalText *self, const char *glyphs, size_t length) {
    if (self == nullptr || !validGlyphs(glyphs, length)) {
        THROW("TerminalText text: null text or invalid glyph span");
        return false;
    }
    (*self).glyphs = length == 0 ? "" : glyphs;
    (*self).length = length;
    return true;
}

const char *TerminalText_getText(const TerminalText *self) { return self ? (*self).glyphs : nullptr; }
size_t TerminalText_getLength(const TerminalText *self) { return self ? (*self).length : 0; }

bool TerminalText_setPen(TerminalText *self, TerminalPen pen) {
    if (self == nullptr) {
        THROW("TerminalText pen: null text");
        return false;
    }
    (*self).pen = pen;
    return true;
}
TerminalPen TerminalText_getPen(const TerminalText *self) {
    return self ? (*self).pen : TerminalPen_0();
}
bool TerminalText_setStyle(TerminalText *self, uint8_t style) {
    if (self == nullptr) {
        THROW("TerminalText style: null text");
        return false;
    }
    TerminalPen *pen = &(*self).pen;
    (*pen).style = style;
    return true;
}
uint8_t TerminalText_getStyle(const TerminalText *self) {
    if (self == nullptr)
        return TERMINAL_STYLE_NONE;
    const TerminalPen *pen = &(*self).pen;
    return (*pen).style;
}
bool TerminalText_setForegroundColor(TerminalText *self, uint32_t color) {
    if (self == nullptr) {
        THROW("TerminalText foreground: null text");
        return false;
    }
    TerminalPen *pen = &(*self).pen;
    (*pen).fg = color;
    return true;
}
uint32_t TerminalText_getForegroundColor(const TerminalText *self) {
    if (self == nullptr)
        return TERMINAL_COLOR_DEFAULT;
    const TerminalPen *pen = &(*self).pen;
    return (*pen).fg;
}
bool TerminalText_setBackgroundColor(TerminalText *self, uint32_t color) {
    if (self == nullptr) {
        THROW("TerminalText background: null text");
        return false;
    }
    TerminalPen *pen = &(*self).pen;
    (*pen).bg = color;
    return true;
}
uint32_t TerminalText_getBackgroundColor(const TerminalText *self) {
    if (self == nullptr)
        return TERMINAL_COLOR_DEFAULT;
    const TerminalPen *pen = &(*self).pen;
    return (*pen).bg;
}

bool TerminalText_setBrackets(TerminalText *self, uint32_t left, uint32_t right) {
    if (self == nullptr) {
        THROW("TerminalText brackets: null text");
        return false;
    }
    (*self).bracketLeft = left;
    (*self).bracketRight = right;
    return true;
}
uint32_t TerminalText_getBracketLeft(const TerminalText *self) {
    return self ? (*self).bracketLeft : 0;
}
uint32_t TerminalText_getBracketRight(const TerminalText *self) {
    return self ? (*self).bracketRight : 0;
}

bool TerminalText_setAlign(TerminalText *self, TerminalTextAlign align) {
    if (self == nullptr || !validAlign(align)) {
        THROW("TerminalText align: null text or unknown alignment");
        return false;
    }
    (*self).align = align;
    return true;
}
TerminalTextAlign TerminalText_getAlign(const TerminalText *self) {
    return self ? (*self).align : TERMINAL_TEXT_ALIGN_LEFT;
}

// Cells the body occupies, using the canvas's locale-free width rule so a wide
// glyph counts as two and the brackets line up with what paint actually writes.
static size_t bodyCells(const char *glyphs, size_t length) {
    size_t cells = 0;
    size_t at = 0;
    while (at < length) {
        TerminalCanvasDecodedScalar decoded = TerminalCanvas_utf8Decode(glyphs + at, length - at);
        if (decoded.bytes == 0) {
            ++cells;   // malformed byte: paint writes '?', one cell
            ++at;
            continue;
        }
        cells += TerminalCanvas_widthOf(decoded.codepoint);
        at += decoded.bytes;
    }
    return cells;
}

size_t TerminalText_measure(const TerminalText *self) {
    if (self == nullptr)
        return 0;
    size_t cells = bodyCells((*self).glyphs, (*self).length);
    if ((*self).bracketLeft != 0)
        ++cells;
    if ((*self).bracketRight != 0)
        ++cells;
    return cells;
}

void TerminalText_paint(const TerminalText *self, TerminalCanvas *region, TerminalPen parentPen) {
    if (self == nullptr || !TerminalCanvas_isValid(region))
        return;
    size_t regionWidth = TerminalCanvas_getWidth(region);
    size_t cells = TerminalText_measure(self);
    size_t x = 0;
    if (cells < regionWidth) {
        if ((*self).align == TERMINAL_TEXT_ALIGN_CENTER)
            x = (regionWidth - cells) / 2;
        else if ((*self).align == TERMINAL_TEXT_ALIGN_RIGHT)
            x = regionWidth - cells;
    }
    TerminalPen pen = resolvePen(&(*self).pen, parentPen);
    if ((*self).bracketLeft != 0) {
        TerminalCanvas_glyph(region, x, 0, (*self).bracketLeft, pen);
        ++x;
    }
    TerminalCanvas_text(region, x, 0, (*self).glyphs, (*self).length, pen);
    x += bodyCells((*self).glyphs, (*self).length);
    if ((*self).bracketRight != 0)
        TerminalCanvas_glyph(region, x, 0, (*self).bracketRight, pen);
}

TerminalProjection TerminalText_toString(const TerminalText *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    const TerminalPen *pen = &(*self).pen;
    return TerminalProjection_format("TerminalText{cells=%zu,brackets=%d,align=%d,style=0x%02X}",
        TerminalText_measure(self), (*self).bracketLeft != 0 || (*self).bracketRight != 0,
        (*self).align, (*pen).style);
}

TerminalProjection TerminalText_toStringStruct(const TerminalText *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    const TerminalPen *pen = &(*self).pen;
    return TerminalProjection_format(
        "TerminalText{glyphs=<borrowed:%zu bytes>,length=%zu,pen={style=0x%02X,fg=0x%08X,bg=0x%08X},bracketLeft=U+%04X,bracketRight=U+%04X,align=%d}",
        (*self).length, (*self).length, (*pen).style, (*pen).fg, (*pen).bg,
        (*self).bracketLeft, (*self).bracketRight, (*self).align);
}
