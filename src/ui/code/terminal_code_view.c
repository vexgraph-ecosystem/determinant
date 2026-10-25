#include "ui/code/terminal_code_view.h"
#include "format/terminal_style.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

;;DEFINITION
/** TerminalCodeView paints one borrowed source-code line into terminal cells.
 * The parser and language adapter stay outside this R3 toolkit: they hand the
 * view ordered byte ranges with pens, and optional virtual hint labels anchored
 * at source byte offsets. The renderer turns those bytes into clipped cell
 * segments, preserving UTF-8 scalar boundaries and cell-based horizontal scroll.
 * Hints are presentation only; they never modify or masquerade as source text.
 *
 * Admission is cold and validates every span, hint, and UTF-8 source boundary
 * before replacing the previous borrowed line. Rendering is hot: it traverses
 * only the admitted arrays and writes cells without allocation, parsing, or log.
 * The host keeps text and arrays alive until its next setLine or free call. */

;;OVERVIEW
/** ============================================================================
 * CLASS: TerminalCodeView (ui/code/terminal_code_view.c)
 * STRUCT FIELDS (Mirroring ui/code/terminal_code_view.h)
 *   const char *text                    borrowed UTF-8 source line
 *   size_t length                       source byte length
 *   const TerminalCodeSpan *spans       borrowed ordered style ranges
 *   size_t spanCount                    number of admitted style ranges
 *   const TerminalCodeHint *hints       borrowed sorted virtual labels
 *   size_t hintCount                    number of admitted hints
 *   size_t scroll                       horizontal viewport offset in cells
 * SLOT RECORD: TerminalCodeSpan
 *   size_t startByte                    inclusive UTF-8 source byte offset
 *   size_t endByte                      exclusive UTF-8 source byte offset
 *   TerminalPen pen                     style/foreground/background for range
 * SLOT RECORD: TerminalCodeHint
 *   size_t byteOffset                   source byte anchor before rendered label
 *   const char *text                    borrowed virtual label text
 *   size_t length                       borrowed label byte length
 *   TerminalPen pen                     style/foreground/background for label
 * PRIVATE HELPERS
 *   scalarBytes(text, length)           valid UTF-8 scalar byte count, or one
 *   cellWidth(text, length)             scalar's terminal-cell width
 *   penAt(self, byte)                   admitted span pen or default pen
 *   paintRange(self, canvas, ...)       clip and draw one source byte range
 * FUNCTION REGISTRY (exports)
 *   Constructor: TerminalCodeView *TerminalCodeView_0(void), chooser, zero
 *   Lifecycle: void TerminalCodeView_free(self)
 *   Admission: bool TerminalCodeView_setLine(self, text, length, spans, count,
 *                                            hints, count)
 *              getters return borrowed source/span/hint views and counts
 *   Scroll: bool TerminalCodeView_setScroll(self, cells)
 *           size_t TerminalCodeView_getScroll(self)
 *   Render: void TerminalCodeView_render(self, canvas, x, y)
 *   Projections: TerminalProjection TerminalCodeView_toString/toStringStruct(self)
 * CONTRACT:
 *   - Source, spans and hints are borrowed; invalid replacement preserves them.
 *   - Spans are ordered/non-overlapping; endpoints must be UTF-8 boundaries.
 *   - Hints are ordered by byte offset, may share an offset, and are virtual.
 *   - Horizontal offsets and render clipping count display cells, not bytes.
 *   - A null/invalid canvas or null receiver makes render a quiet no-op.
 *   - The hot renderer allocates nothing and does not validate admitted input.
 * ============================================================================ */

struct TerminalCodeView {
    const char *text;
    size_t length;
    const TerminalCodeSpan *spans;
    size_t spanCount;
    const TerminalCodeHint *hints;
    size_t hintCount;
    size_t scroll;
};

static size_t scalarBytes(const char *text, size_t length) {
    TerminalCanvasDecodedScalar decoded = TerminalCanvas_utf8Decode(text, length);
    return decoded.bytes == 0 ? 1u : decoded.bytes;
}

static size_t cellWidth(const char *text, size_t length) {
    TerminalCanvasDecodedScalar decoded = TerminalCanvas_utf8Decode(text, length);
    return decoded.bytes == 0 ? 1u : TerminalCanvas_widthOf(decoded.codepoint);
}

static bool isBoundary(const char *text, size_t length, size_t offset) {
    if (offset > length)
        return false;
    size_t at = 0;
    while (at < offset) {
        TerminalCanvasDecodedScalar decoded = TerminalCanvas_utf8Decode(text + at,
                                                                          length - at);
        if (decoded.bytes == 0)
            return false;
        size_t bytes = scalarBytes(text + at, length - at);
        if (bytes > offset - at)
            return false;
        at += bytes;
    }
    return at == offset;
}

static bool validSpans(const char *text, size_t length,
                       const TerminalCodeSpan *spans, size_t count) {
    size_t previousEnd = 0;
    for (size_t index = 0; index < count; ++index) {
        const TerminalCodeSpan *span = &spans[index];
        if ((*span).startByte > (*span).endByte || (*span).endByte > length
            || (*span).startByte < previousEnd
            || !isBoundary(text, length, (*span).startByte)
            || !isBoundary(text, length, (*span).endByte))
            return false;
        previousEnd = (*span).endByte;
    }
    return true;
}

static bool validHints(const char *text, size_t length,
                       const TerminalCodeHint *hints, size_t count) {
    size_t previousOffset = 0;
    for (size_t index = 0; index < count; ++index) {
        const TerminalCodeHint *hint = &hints[index];
        if ((*hint).byteOffset > length || (*hint).text == nullptr
            || (*hint).length == 0 || (*hint).byteOffset < previousOffset
            || !isBoundary(text, length, (*hint).byteOffset))
            return false;
        previousOffset = (*hint).byteOffset;
    }
    return true;
}

static TerminalPen penAt(const TerminalCodeView *self, size_t byte) {
    for (size_t index = 0; index < (*self).spanCount; ++index) {
        const TerminalCodeSpan *span = &(*self).spans[index];
        if (byte >= (*span).startByte && byte < (*span).endByte)
            return (*span).pen;
        if (byte < (*span).startByte)
            break;
    }
    return TerminalPen_0();
}

static void paintRange(const TerminalCodeView *self, TerminalCanvas *canvas,
                       size_t x, size_t y, size_t first, size_t end,
                       size_t *cell, size_t left, size_t right) {
    size_t at = first;
    while (at < end) {
        size_t bytes = scalarBytes((*self).text + at, end - at);
        size_t width = cellWidth((*self).text + at, end - at);
        if ((*cell) >= left && (*cell) + width <= right)
            TerminalCanvas_text(canvas, x + (*cell) - left, y,
                                (*self).text + at, bytes, penAt(self, at));
        (*cell) += width;
        at += bytes;
    }
}

TerminalCodeView *TerminalCodeView_0(void) {
    return calloc(1u, sizeof(TerminalCodeView));
}

void TerminalCodeView_free(TerminalCodeView *self) {
    free(self);
}

bool TerminalCodeView_setLine(TerminalCodeView *self, const char *text, size_t length,
                              const TerminalCodeSpan *spans, size_t spanCount,
                              const TerminalCodeHint *hints, size_t hintCount) {
    const char *admittedText = text == nullptr ? "" : text;
    if (self == nullptr || (text == nullptr && length != 0)
        || (spans == nullptr && spanCount != 0) || (hints == nullptr && hintCount != 0)
        || !isBoundary(admittedText, length, length)
        || !validSpans(admittedText, length, spans, spanCount)
        || !validHints(admittedText, length, hints, hintCount))
        return false;
    (*self).text = admittedText;
    (*self).length = length;
    (*self).spans = spans;
    (*self).spanCount = spanCount;
    (*self).hints = hints;
    (*self).hintCount = hintCount;
    (*self).scroll = 0;
    return true;
}

bool TerminalCodeView_setScroll(TerminalCodeView *self, size_t cells) {
    if (self == nullptr)
        return false;
    (*self).scroll = cells;
    return true;
}

size_t TerminalCodeView_getScroll(const TerminalCodeView *self) {
    return self == nullptr ? 0u : (*self).scroll;
}

const char *TerminalCodeView_getText(const TerminalCodeView *self) {
    return self == nullptr ? nullptr : (*self).text;
}

size_t TerminalCodeView_getLength(const TerminalCodeView *self) {
    return self == nullptr ? 0u : (*self).length;
}

const TerminalCodeSpan *TerminalCodeView_getSpans(const TerminalCodeView *self) {
    return self == nullptr ? nullptr : (*self).spans;
}

size_t TerminalCodeView_getSpanCount(const TerminalCodeView *self) {
    return self == nullptr ? 0u : (*self).spanCount;
}

const TerminalCodeHint *TerminalCodeView_getHints(const TerminalCodeView *self) {
    return self == nullptr ? nullptr : (*self).hints;
}

size_t TerminalCodeView_getHintCount(const TerminalCodeView *self) {
    return self == nullptr ? 0u : (*self).hintCount;
}

void TerminalCodeView_render(const TerminalCodeView *self, TerminalCanvas *canvas,
                             size_t x, size_t y) {
    if (self == nullptr || !TerminalCanvas_isValid(canvas)
        || x >= TerminalCanvas_getWidth(canvas) || y >= TerminalCanvas_getHeight(canvas))
        return;
    size_t width = TerminalCanvas_getWidth(canvas) - x;
    size_t left = (*self).scroll;
    size_t right = left > SIZE_MAX - width ? SIZE_MAX : left + width;
    size_t cell = 0;
    size_t at = 0;
    size_t hintIndex = 0;
    while (at <= (*self).length) {
        while (hintIndex < (*self).hintCount
               && (*self).hints[hintIndex].byteOffset == at) {
            const TerminalCodeHint *hint = &(*self).hints[hintIndex++];
            size_t hintAt = 0;
            while (hintAt < (*hint).length) {
                size_t bytes = scalarBytes((*hint).text + hintAt, (*hint).length - hintAt);
                size_t glyphWidth = cellWidth((*hint).text + hintAt, (*hint).length - hintAt);
                if (cell >= left && cell + glyphWidth <= right)
                    TerminalCanvas_text(canvas, x + cell - left, y,
                                        (*hint).text + hintAt, bytes, (*hint).pen);
                cell += glyphWidth;
                hintAt += bytes;
            }
        }
        if (at == (*self).length)
            break;
        size_t next = at + scalarBytes((*self).text + at, (*self).length - at);
        paintRange(self, canvas, x, y, at, next, &cell, left, right);
        at = next;
    }
}

TerminalProjection TerminalCodeView_toString(const TerminalCodeView *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format("TerminalCodeView(bytes=%zu, spans=%zu, hints=%zu, scroll=%zu)",
                                     (*self).length, (*self).spanCount,
                                     (*self).hintCount, (*self).scroll);
}

TerminalProjection TerminalCodeView_toStringStruct(const TerminalCodeView *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format("TerminalCodeView { text=%p, length=%zu, spans=%p, spanCount=%zu, hints=%p, hintCount=%zu, scroll=%zu }",
                                     (void*) (*self).text, (*self).length,
                                     (void*) (*self).spans, (*self).spanCount,
                                     (void*) (*self).hints, (*self).hintCount,
                                     (*self).scroll);
}
