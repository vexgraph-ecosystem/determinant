#include "terminal/terminal_text_view.h"
#include "c23/constructor.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** A generic text viewport: borrowed lines drawn top-aligned into a region with
 * a scroll offset that defaults to "stick to the tail". No wrapping, no speaker
 * labels, no transcript policy — those are the application's. Owns no memory. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalTextView (terminal/terminal_text_view.c)
 * STRUCT FIELDS (Mirroring terminal/terminal_text_view.h)
 * const char *const *lines  borrowed lines (one row each)
 * size_t count              line count
 * size_t scroll             lines scrolled up from the tail
 * bool tail                 keep the newest line visible (default true)
 * TerminalPen pen           text pen
 * FUNCTION REGISTRY (exports)
 * Constructors: TerminalTextView_0(void), TerminalTextView_2(lines, count)
 * Config: set/getLines, set/getScroll, setTail/isTail, set/getPen
 * Navigation: bool TerminalTextView_scrollBy(self, delta, visibleRows)
 * Render: TerminalTextView_render(self, canvas)
 * Projections: bool TerminalTextView_toString/toStringStruct(...)
 * CONTRACT: null/invalid canvas no-ops; lines past the region clip; a null line
 * renders blank; render never allocates or logs.
 * ======================================================================== */

TerminalTextView TerminalTextView_0(void) {
    TerminalTextView view = {0};
    view.tail = true;
    view.pen = TerminalPen_0();
    return view;
}

TerminalTextView TerminalTextView_2(const char *const *lines, size_t count) {
    TerminalTextView view = TerminalTextView_0();
    view.lines = (count != 0) ? lines : nullptr;
    view.count = (lines != nullptr) ? count : 0;
    return view;
}

bool TerminalTextView_setLines(TerminalTextView *self, const char *const *lines, size_t count) {
    if (self == nullptr || (count != 0 && lines == nullptr)) {
        THROW("TerminalTextView lines: null list");
        return false;
    }
    (*self).lines = lines;
    (*self).count = count;
    return true;
}
void TerminalTextView_getLines(const TerminalTextView *self, const char *const **outLines, size_t *outCount) {
    if (outLines) *outLines = self ? (*self).lines : nullptr;
    if (outCount) *outCount = self ? (*self).count : 0;
}
bool TerminalTextView_setScroll(TerminalTextView *self, size_t scroll) {
    if (self == nullptr) { THROW("TerminalTextView scroll: null"); return false; }
    (*self).scroll = scroll;
    return true;
}
size_t TerminalTextView_getScroll(const TerminalTextView *self) { return self ? (*self).scroll : 0; }
void TerminalTextView_setTail(TerminalTextView *self, bool tail) { if (self != nullptr) (*self).tail = tail; }
bool TerminalTextView_isTail(const TerminalTextView *self) { return self && (*self).tail; }
void TerminalTextView_setPen(TerminalTextView *self, TerminalPen pen) { if (self != nullptr) (*self).pen = pen; }
TerminalPen TerminalTextView_getPen(const TerminalTextView *self) { return self ? (*self).pen : TerminalPen_0(); }

bool TerminalTextView_scrollBy(TerminalTextView *self, long delta, size_t visibleRows) {
    (void) visibleRows;
    if (self == nullptr)
        return false;
    if (delta > 0) {
        if ((*self).scroll == 0)
            return false;
        size_t step = (size_t) delta;
        (*self).scroll = step < (*self).scroll ? (*self).scroll - step : 0;
        return true;
    }
    if (delta < 0) {
        (*self).scroll += (size_t) -delta;
        return true;
    }
    return false;
}

void TerminalTextView_render(const TerminalTextView *self, TerminalCanvas *canvas) {
    if (self == nullptr || !TerminalCanvas_isValid(canvas))
        return;
    size_t rows = TerminalCanvas_getHeight(canvas);
    size_t first = 0;
    if ((*self).count > rows)
        first = (*self).count - rows;
    first = (*self).scroll <= first ? first - (*self).scroll : 0;
    for (size_t row = 0; row < rows && first + row < (*self).count; ++row) {
        const char *line = (*self).lines[first + row];
        if (line != nullptr)
            TerminalCanvas_text(canvas, 0, row, line, strlen(line), (*self).pen);
    }
}

bool TerminalTextView_toString(const TerminalTextView *self, char *dest, size_t cap, bool *outTruncated) {
    if (outTruncated != nullptr)
        *outTruncated = false;
    if (dest == nullptr || cap == 0) {
        if (outTruncated != nullptr)
            *outTruncated = true;
        THROW("TerminalTextView projection: missing destination");
        return false;
    }
    int n = self ? snprintf(dest, cap, "TerminalTextView{count=%zu,scroll=%zu,tail=%d}",
                            (*self).count, (*self).scroll, (*self).tail)
                 : snprintf(dest, cap, "nullptr");
    bool cut = n < 0 || (size_t) n >= cap;
    if (outTruncated != nullptr)
        *outTruncated = cut;
    if (cut)
        THROW("TerminalTextView projection: truncated at %zu bytes", cap);
    return !cut;
}

bool TerminalTextView_toStringStruct(const TerminalTextView *self, char *dest, size_t cap, bool *outTruncated) {
    return TerminalTextView_toString(self, dest, cap, outTruncated);
}
