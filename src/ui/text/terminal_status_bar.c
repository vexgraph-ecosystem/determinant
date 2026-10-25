#include "ui/text/terminal_status_bar.h"
#include "terminal/terminal_style.h"
#include "c23/constructor.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** A generic one-row status line: a borrowed left string and an optional right
 * string, drawn so the right only appears when it cannot collide with the left.
 * No meaning is attached to either string. Owns no memory. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalStatusBar (ui/text/terminal_status_bar.c)
 * STRUCT FIELDS (Mirroring ui/text/terminal_status_bar.h)
 * const char *left, *right  borrowed status strings (right nullable)
 * TerminalPen leftPen, rightPen  pens
 * FUNCTION REGISTRY (exports)
 * Constructors: TerminalStatusBar_0(void), TerminalStatusBar_1(left)
 * Config: set/getLeft, set/getRight, set/getPens
 * Render: TerminalStatusBar_render(self, canvas)
 * Projections: TerminalProjection TerminalStatusBar_toString/toStringStruct(self)
 * CONTRACT: null/invalid canvas no-ops; the left string clips at the region
 * width; the right string is dropped when it would touch the left; no
 * allocation, no logging.
 * ======================================================================== */

TerminalStatusBar TerminalStatusBar_0(void) {
    TerminalStatusBar bar = {0};
    bar.leftPen = TerminalPen_1(TERMINAL_STYLE_MUTED);
    bar.rightPen = TerminalPen_1(TERMINAL_STYLE_MUTED);
    return bar;
}

TerminalStatusBar TerminalStatusBar_1(const char *left) {
    TerminalStatusBar bar = TerminalStatusBar_0();
    bar.left = left;
    return bar;
}

bool TerminalStatusBar_setLeft(TerminalStatusBar *self, const char *left) {
    if (self == nullptr) { THROW("TerminalStatusBar left: null self"); return false; }
    (*self).left = left;
    return true;
}
const char *TerminalStatusBar_getLeft(const TerminalStatusBar *self) { return self ? (*self).left : nullptr; }
bool TerminalStatusBar_setRight(TerminalStatusBar *self, const char *right) {
    if (self == nullptr) { THROW("TerminalStatusBar right: null self"); return false; }
    (*self).right = right;
    return true;
}
const char *TerminalStatusBar_getRight(const TerminalStatusBar *self) { return self ? (*self).right : nullptr; }
void TerminalStatusBar_setPens(TerminalStatusBar *self, TerminalPen leftPen, TerminalPen rightPen) {
    if (self != nullptr) { (*self).leftPen = leftPen; (*self).rightPen = rightPen; }
}
TerminalStatusBarPens TerminalStatusBar_getPens(const TerminalStatusBar *self) {
    return self ? (TerminalStatusBarPens) { (*self).leftPen, (*self).rightPen }
                : (TerminalStatusBarPens) { TerminalPen_0(), TerminalPen_0() };
}

void TerminalStatusBar_render(const TerminalStatusBar *self, TerminalCanvas *canvas) {
    if (self == nullptr || !TerminalCanvas_isValid(canvas))
        return;
    size_t width = TerminalCanvas_getWidth(canvas);
    size_t leftLen = (*self).left ? strlen((*self).left) : 0;
    size_t rightLen = (*self).right ? strlen((*self).right) : 0;
    bool rightFits = rightLen > 0 && leftLen + rightLen + 1 <= width;
    size_t leftCap = rightFits ? width - rightLen - 1 : width;
    if ((*self).left != nullptr && leftCap > 0) {
        TerminalCanvas region = TerminalCanvas_sub(canvas, 0, 0, leftCap, 1);
        TerminalCanvas_text(&region, 0, 0, (*self).left, leftLen, (*self).leftPen);
    }
    if (rightFits)
        TerminalCanvas_text(canvas, width - rightLen, 0, (*self).right, rightLen, (*self).rightPen);
}

TerminalProjection TerminalStatusBar_toString(const TerminalStatusBar *self) {
    return TerminalStatusBar_toStringStruct(self);
}

TerminalProjection TerminalStatusBar_toStringStruct(const TerminalStatusBar *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format("TerminalStatusBar{left=\"%s\",right=\"%s\"}",
                                     (*self).left ? (*self).left : "",
                                     (*self).right ? (*self).right : "");
}
