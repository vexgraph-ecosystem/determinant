#include "terminal/terminal_markdown_view.h"
#include "terminal/terminal_style.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

;;DEFINITION
/** A rendered Markdown document in terminal cells. It exists because a README is
 * not source code: a preview has to spend the cell budget on hierarchy, not on
 * the punctuation that describes it, and a terminal has no proportional type to
 * fall back on. So headings become weight and accent, emphasis becomes bold and
 * underline, code becomes a tinted run, lists become hanging indents, quotes
 * become bars, and fences become a ruled block. The syntax markers themselves are
 * consumed by the parser and never printed.
 *
 * The split is deliberate. Parsing and wrapping are COLD: setViewport(width)
 * walks the borrowed source once, produces flat rows and segments, and caches
 * them. render() is HOT: it walks the visible slice and writes cells, with no
 * allocation, no logging and no parsing. A resize asks for a new layout; a
 * scroll asks for nothing but a different row index. That is what lets the shell
 * keep its glide and present-on-demand path intact while previewing.
 *
 * This is a GFM-ish subset, not a CommonMark engine. It understands ATX and
 * setext headings, paragraphs, strong/emphasis, inline code, links, images,
 * unordered/ordered/task lists, block quotes, fenced code, thematic breaks and
 * simple pipe tables. It does not lay out HTML, render images, resolve reference
 * links or footnotes, do math, nest tables, or shape grapheme clusters. Those are
 * stated limits, not accidents: the renderer must never pretend to a conformance
 * it does not have.
 *
 * Borrowing is explicit. The source lines belong to the caller; the view stores
 * only the pointer array and every span points into it. The caller clears the
 * source before freeing it. The style record is borrowed by value and resolved at
 * render time, so recolouring does not reflow. */

;;OVERVIEW
/** ============================================================================
 * CLASS: TerminalMarkdownView (terminal/terminal_markdown_view.c)
 * STRUCT FIELDS (Mirroring terminal/terminal_markdown_view.h)
 *   const char *const *lines   borrowed source lines; caller owns their bytes
 *   size_t lineCount           number of borrowed lines
 *   bool dirty                 source changed since the last layout
 *   bool ready                 a layout exists and may be rendered
 *   bool truncated             the wrapped-row limit was reached
 *   bool failed                a cold pool growth was refused
 *   size_t width               content width the current layout used
 *   size_t visibleRows         rows the caller can show (for the scroll clamp)
 *   size_t scroll              first wrapped row drawn
 *   MarkdownRow *rows          owned flat wrapped-row table
 *   size_t rowCount            rows in the document (blank rows included)
 *   size_t rowCapacity         admitted row slots
 *   MarkdownSegment *segments  owned flat segment pool
 *   size_t segmentCount        live segments
 *   size_t segmentCapacity     admitted segment slots
 *   TerminalMarkdownStyle style  role pens resolved at render time
 *   size_t refusals            cold allocation refusals in this epoch
 * SLOT RECORD: MarkdownSegment
 *   const char *text           borrowed source span (or a static glyph)
 *   size_t length              bytes in `text`
 *   size_t repeat              how many times `text` is drawn (1 for normal text)
 *   TerminalMarkdownRole role  which style pen supplies the base colour
 *   uint8_t style              extra BOLD/UNDERLINE flags
 * SLOT RECORD: MarkdownRow
 *   size_t firstSegment        offset of this row's first segment
 *   size_t segmentCount        number of segments in this row
 * PRIVATE HELPERS
 *   cellsOf/bytesForCells(text, length, ...)  cell-aware UTF-8 measurement
 *   refuse(self, reason)      count a cold refusal, reporting once per epoch
 *   growRows/growSegments(self)  cold doubling growth preserving live entries
 *   appendRow/appendRowForced(self)  begin a wrapped row (the notice may bypass
 *                             the document limit by one)
 *   appendSegment(self, ...)  append to the open row
 *   prefixSpaces/prefixQuote/prefixBeginMarker/prefixEndMarker
 *   wrapStart/wrapContinuation/wrapSpace/wrapWord/wrapBreak
 *   findBytes/findBacktickRun/emitInline
 *   emitGap/emitHeading/emitParagraph/emitFence/emitRule/emitList/emitQuote
 *   emitTable/splitTableRow/trimSpan
 *   layout(self)              the cold parse + wrap pass
 * FUNCTION REGISTRY (exports)
 * Constructor: TerminalMarkdownView *TerminalMarkdownView_0(void) [arity chooser]
 * Lifecycle:   void TerminalMarkdownView_free(self)
 * Source:      bool setSource(self, lines, count)
 * Layout:      bool setViewport(self, width)
 * Viewport:    bool setVisibleRows(self, rows); size_t visibleRows(self)
 *              size_t maxScroll(self)
 * Scroll:      bool setScroll(self, row); size_t getScroll(self)
 *              bool scrollBy(self, delta)
 * Queries:     size_t rowCount/getWidth(self)
 *              bool isReady/isTruncated(self)
 *              size_t exhaustionCount(self); void clearExhaustion(self)
 * Style:       bool setStyle(self, style); TerminalMarkdownStyle getStyle(self)
 * Render:      void TerminalMarkdownView_render(self, canvas)
 * Projections: TerminalProjection TerminalMarkdownView_toString/toStringStruct(self)
 * CONTRACT:
 *   - setSource borrows; the caller keeps the line bytes alive and clears the
 *     source before freeing them. A null list or line with a nonzero count is refused.
 *   - setViewport(0) is refused; a null view is refused on every setter.
 *   - render is quiet and allocation-free; a null view, invalid canvas or an
 *     unready view is a no-op.
 *   - scroll is clamped in the view; a caller cannot store an offset past the
 *     document and wonder why nothing moved.
 *   - row growth refusal is counted and reported once per epoch; the view then
 *     reports not-ready rather than rendering half a document.
 *   - the row limit is an honest truncation, not a hidden cap: isTruncated()
 *     tells the caller, and a final marker row says so when there is room.
 * ============================================================================ */

#define MARKDOWN_SPACES_CAP 32u
#define MARKDOWN_PREFIX_ITEMS 16u
#define MARKDOWN_MAX_COLUMNS 16u
#define MARKDOWN_MAX_COLUMN_CELLS 32u
#define MARKDOWN_MAX_INLINE_DEPTH 8u
#define MARKDOWN_NO_INDEX ((size_t) -1)

static const char *const MARKDOWN_SPACES =
    "                                ";
static const char *const MARKDOWN_RULE_GLYPH = "─";
static const char *const MARKDOWN_QUOTE_GLYPH = "│ ";
static const char *const MARKDOWN_IMAGE_GLYPH = "▧ ";

typedef struct MarkdownSegment {
    const char *text;
    size_t length;
    size_t repeat;
    TerminalMarkdownRole role;
    uint8_t style;
} MarkdownSegment;

typedef struct MarkdownRow {
    size_t firstSegment;
    size_t segmentCount;
} MarkdownRow;

typedef struct MarkdownPrefixItem {
    const char *text;
    size_t length;
    size_t cells;
    TerminalMarkdownRole role;
    uint8_t style;
} MarkdownPrefixItem;

typedef struct MarkdownPrefix {
    MarkdownPrefixItem items[MARKDOWN_PREFIX_ITEMS];
    size_t count;
    size_t markerStart;
    size_t markerCount;
    size_t cells;
    size_t hang;
} MarkdownPrefix;

typedef struct MarkdownWrap {
    const MarkdownPrefix *prefix;
    size_t width;
    size_t x;
    size_t prefixCells;
    bool open;
    bool started;
    bool pendingSpace;
    bool content;
} MarkdownWrap;

typedef struct MarkdownListMarker {
    bool valid;
    bool ordered;
    bool task;
    bool taskDone;
    size_t indent;
    size_t contentStart;
    size_t number;
} MarkdownListMarker;

struct TerminalMarkdownView {
    const char *const *lines;
    size_t lineCount;
    bool dirty;
    bool ready;
    bool truncated;
    bool failed;
    size_t width;
    size_t visibleRows;
    size_t scroll;
    MarkdownRow *rows;
    size_t rowCount;
    size_t rowCapacity;
    MarkdownSegment *segments;
    size_t segmentCount;
    size_t segmentCapacity;
    TerminalMarkdownStyle style;
    size_t refusals;
};

static size_t cellsOf(const char *text, size_t length) {
    size_t cells = 0;
    size_t at = 0;
    while (at < length) {
        TerminalCanvasDecodedScalar decoded = TerminalCanvas_utf8Decode(text + at, length - at);
        if (decoded.bytes == 0) {
            ++cells;
            ++at;
            continue;
        }
        cells += TerminalCanvas_widthOf(decoded.codepoint);
        at += decoded.bytes;
    }
    return cells;
}

// Bytes that fit in `cells` CELLS, never splitting a UTF-8 scalar. A combining
// mark has zero width and rides with the scalar before it, so a row cannot start
// with a floating accent.
static size_t bytesForCells(const char *text, size_t length, size_t cells, size_t *usedCells) {
    size_t at = 0;
    size_t consumed = 0;
    while (at < length) {
        TerminalCanvasDecodedScalar decoded = TerminalCanvas_utf8Decode(text + at, length - at);
        size_t width = decoded.bytes == 0 ? 1u : TerminalCanvas_widthOf(decoded.codepoint);
        if (width != 0 && consumed + width > cells)
            break;
        consumed += width;
        at += decoded.bytes == 0 ? 1u : decoded.bytes;
    }
    if (usedCells != nullptr)
        *usedCells = consumed;
    return at;
}

static size_t scalarBytes(const char *text, size_t length) {
    TerminalCanvasDecodedScalar decoded = TerminalCanvas_utf8Decode(text, length);
    return decoded.bytes == 0 ? 1u : decoded.bytes;
}

static void refuse(TerminalMarkdownView *self, const char *reason) {
    if ((*self).refusals == 0)
        THROW("TerminalMarkdownView exhausted: %s (rows %zu, segments %zu)",
              reason, (*self).rowCount, (*self).segmentCount);
    if ((*self).refusals != SIZE_MAX)
        ++(*self).refusals;
    (*self).failed = true;
}

static bool growRows(TerminalMarkdownView *self) {
    size_t next = (*self).rowCapacity == 0 ? TERMINAL_MARKDOWN_ROWS_DEFAULT
                                           : (*self).rowCapacity * 2u;
    if (next < (*self).rowCapacity || next > SIZE_MAX / sizeof(MarkdownRow)) {
        refuse(self, "row capacity overflow");
        return false;
    }
    MarkdownRow *grown = Memory_alloc(TYPE_BYTE_ARRAY, next * sizeof(MarkdownRow));
    if (grown == nullptr) {
        refuse(self, "row pool allocation refused");
        return false;
    }
    if ((*self).rowCapacity != 0)
        memcpy(grown, (*self).rows, (*self).rowCapacity * sizeof(MarkdownRow));
    Memory_free((*self).rows);
    (*self).rows = grown;
    (*self).rowCapacity = next;
    return true;
}

static bool growSegments(TerminalMarkdownView *self) {
    size_t next = (*self).segmentCapacity == 0 ? TERMINAL_MARKDOWN_SEGMENTS_DEFAULT
                                              : (*self).segmentCapacity * 2u;
    if (next < (*self).segmentCapacity || next > SIZE_MAX / sizeof(MarkdownSegment)) {
        refuse(self, "segment capacity overflow");
        return false;
    }
    MarkdownSegment *grown = Memory_alloc(TYPE_BYTE_ARRAY, next * sizeof(MarkdownSegment));
    if (grown == nullptr) {
        refuse(self, "segment pool allocation refused");
        return false;
    }
    if ((*self).segmentCapacity != 0)
        memcpy(grown, (*self).segments, (*self).segmentCapacity * sizeof(MarkdownSegment));
    Memory_free((*self).segments);
    (*self).segments = grown;
    (*self).segmentCapacity = next;
    return true;
}

static bool appendRowInternal(TerminalMarkdownView *self, bool force) {
    if (!force && (*self).rowCount + 1u >= TERMINAL_MARKDOWN_ROWS_LIMIT) {
        (*self).truncated = true;
        return false;
    }
    if ((*self).rowCount == (*self).rowCapacity && !growRows(self))
        return false;
    (*self).rows[(*self).rowCount] = (MarkdownRow) { (*self).segmentCount, 0 };
    ++(*self).rowCount;
    return true;
}

static bool appendRow(TerminalMarkdownView *self) {
    return appendRowInternal(self, false);
}

static bool appendSegment(TerminalMarkdownView *self, const char *text, size_t length,
                          size_t repeat, TerminalMarkdownRole role, uint8_t style) {
    if (self == nullptr || (*self).rowCount == 0 || text == nullptr)
        return false;
    if ((*self).segmentCount == (*self).segmentCapacity && !growSegments(self))
        return false;
    MarkdownRow *row = &(*self).rows[(*self).rowCount - 1u];
    if ((*self).segmentCount != (*row).firstSegment + (*row).segmentCount)
        return false;
    MarkdownSegment *segment = &(*self).segments[(*self).segmentCount];
    (*segment).text = text;
    (*segment).length = length;
    (*segment).repeat = repeat == 0 ? 1u : repeat;
    (*segment).role = role;
    (*segment).style = style;
    ++(*self).segmentCount;
    ++(*row).segmentCount;
    return true;
}

static void prefixAdd(MarkdownPrefix *prefix, const char *text, size_t length,
                      TerminalMarkdownRole role, uint8_t style) {
    if (prefix == nullptr || prefix->count >= MARKDOWN_PREFIX_ITEMS || text == nullptr)
        return;
    MarkdownPrefixItem *item = &prefix->items[prefix->count];
    (*item).text = text;
    (*item).length = length;
    (*item).cells = cellsOf(text, length);
    (*item).role = role;
    (*item).style = style;
    prefix->cells += (*item).cells;
    ++prefix->count;
}

static void prefixSpaces(MarkdownPrefix *prefix, size_t cells,
                         TerminalMarkdownRole role, uint8_t style) {
    while (cells > 0) {
        size_t take = cells > MARKDOWN_SPACES_CAP ? MARKDOWN_SPACES_CAP : cells;
        prefixAdd(prefix, MARKDOWN_SPACES, take, role, style);
        cells -= take;
    }
}

static void prefixQuote(MarkdownPrefix *prefix, size_t depth) {
    for (size_t level = 0; level < depth && prefix->count < MARKDOWN_PREFIX_ITEMS; ++level)
        prefixAdd(prefix, MARKDOWN_QUOTE_GLYPH, strlen(MARKDOWN_QUOTE_GLYPH),
                  TERMINAL_MARKDOWN_QUOTE, 0);
}

static void prefixBeginMarker(MarkdownPrefix *prefix) {
    prefix->markerStart = prefix->count;
    prefix->markerCount = 0;
    prefix->hang = 0;
}

static void prefixEndMarker(MarkdownPrefix *prefix) {
    if (prefix->count < prefix->markerStart)
        prefix->count = prefix->markerStart;
    prefix->markerCount = prefix->count - prefix->markerStart;
    for (size_t index = prefix->markerStart; index < prefix->count; ++index)
        prefix->hang += prefix->items[index].cells;
}

static bool wrapStart(TerminalMarkdownView *self, MarkdownWrap *wrap, bool first) {
    if (!appendRow(self))
        return false;
    wrap->open = true;
    wrap->x = 0;
    wrap->content = false;
    wrap->pendingSpace = false;
    const MarkdownPrefix *prefix = wrap->prefix;
    if (prefix != nullptr) {
        size_t limit = first ? prefix->count : prefix->markerStart;
        for (size_t index = 0; index < limit; ++index) {
            const MarkdownPrefixItem *item = &prefix->items[index];
            if (!appendSegment(self, (*item).text, (*item).length, 1u, (*item).role, (*item).style))
                return false;
            wrap->x += (*item).cells;
        }
        if (!first && prefix->hang != 0) {
            if (!appendSegment(self, MARKDOWN_SPACES, prefix->hang, 1u,
                               TERMINAL_MARKDOWN_BODY, 0))
                return false;
            wrap->x += prefix->hang;
        }
    }
    wrap->prefixCells = wrap->x;
    wrap->started = true;
    return true;
}

static void wrapSpace(MarkdownWrap *wrap) {
    if (wrap != nullptr)
        wrap->pendingSpace = true;
}

static bool wrapWord(TerminalMarkdownView *self, MarkdownWrap *wrap, const char *text,
                     size_t length, TerminalMarkdownRole role, uint8_t style) {
    if (wrap == nullptr || text == nullptr || length == 0)
        return true;
    if (!wrap->open && !wrapStart(self, wrap, !wrap->started))
        return false;
    size_t wordCells = cellsOf(text, length);
    size_t gap = wrap->pendingSpace && wrap->content ? 1u : 0u;
    if (wrap->content && wrap->x + gap + wordCells > wrap->width) {
        if (!wrapStart(self, wrap, false))
            return false;
        gap = 0;
    }
    if (gap != 0) {
        if (!appendSegment(self, " ", 1u, 1u, TERMINAL_MARKDOWN_BODY, 0))
            return false;
        ++wrap->x;
    }
    wrap->pendingSpace = false;
    size_t available = wrap->width > wrap->x ? wrap->width - wrap->x : 0u;
    if (wordCells <= available) {
        if (!appendSegment(self, text, length, 1u, role, style))
            return false;
        wrap->x += wordCells;
        wrap->content = true;
        return true;
    }
    size_t at = 0;
    while (at < length) {
        available = wrap->width > wrap->x ? wrap->width - wrap->x : 0u;
        if (available == 0) {
            if (!wrapStart(self, wrap, false))
                return false;
            available = wrap->width > wrap->x ? wrap->width - wrap->x : 1u;
        }
        size_t used = 0;
        size_t take = bytesForCells(text + at, length - at, available, &used);
        if (take == 0) {
            take = scalarBytes(text + at, length - at);
            used = cellsOf(text + at, take);
        }
        if (!appendSegment(self, text + at, take, 1u, role, style))
            return false;
        wrap->x += used;
        wrap->content = true;
        at += take;
        if (at < length && !wrapStart(self, wrap, false))
            return false;
    }
    return true;
}

static bool wrapBreak(TerminalMarkdownView *self, MarkdownWrap *wrap) {
    if (wrap == nullptr || !wrap->open || !wrap->content)
        return true;
    wrap->pendingSpace = false;
    return wrapStart(self, wrap, false);
}

static size_t findBytes(const char *text, size_t length, size_t from,
                        const char *needle, size_t needleLength) {
    if (needleLength == 0 || from >= length || needleLength > length - from)
        return MARKDOWN_NO_INDEX;
    for (size_t at = from; at + needleLength <= length; ++at)
        if (memcmp(text + at, needle, needleLength) == 0)
            return at;
    return MARKDOWN_NO_INDEX;
}

static size_t findBacktickRun(const char *text, size_t length, size_t from, size_t run) {
    size_t at = from;
    while (at < length) {
        if (text[at] != '`') {
            ++at;
            continue;
        }
        size_t count = 0;
        while (at + count < length && text[at + count] == '`')
            ++count;
        if (count == run)
            return at;
        at += count;
    }
    return MARKDOWN_NO_INDEX;
}

static bool isPunctuation(char character) {
    return (character >= '!' && character <= '/') || (character >= ':' && character <= '@')
        || (character >= '[' && character <= '`') || (character >= '{' && character <= '~');
}

static bool isSpace(char character) {
    return character == ' ' || character == '\t';
}

static bool appendPlain(TerminalMarkdownView *self, MarkdownWrap *wrap, const char *text,
                        size_t length, TerminalMarkdownRole role, uint8_t style) {
    size_t at = 0;
    while (at < length) {
        if (isSpace(text[at])) {
            wrapSpace(wrap);
            ++at;
            continue;
        }
        size_t start = at;
        while (at < length && !isSpace(text[at]))
            ++at;
        if (!wrapWord(self, wrap, text + start, at - start, role, style))
            return false;
    }
    return true;
}

static bool emitInline(TerminalMarkdownView *self, MarkdownWrap *wrap, const char *text,
                       size_t length, TerminalMarkdownRole base, uint8_t style, size_t depth);

static bool emitLink(TerminalMarkdownView *self, MarkdownWrap *wrap, const char *text,
                     size_t length, size_t open, size_t close, uint8_t style, size_t depth) {
    const char *label = text + open + 1u;
    size_t labelLength = close - open - 1u;
    size_t paren = findBytes(text, length, close, "](", 2u);
    if (paren == MARKDOWN_NO_INDEX)
        return appendPlain(self, wrap, text + open, 1u, TERMINAL_MARKDOWN_LINK, style);
    size_t targetStart = paren + 2u;
    size_t targetEnd = findBytes(text, length, targetStart, ")", 1u);
    if (targetEnd == MARKDOWN_NO_INDEX)
        return appendPlain(self, wrap, text + open, 1u, TERMINAL_MARKDOWN_LINK, style);
    if (labelLength != 0) {
        if (!emitInline(self, wrap, label, labelLength, TERMINAL_MARKDOWN_LINK, style, depth + 1u))
            return false;
    } else if (!appendPlain(self, wrap, text + targetStart, targetEnd - targetStart,
                            TERMINAL_MARKDOWN_LINK, style)) {
        return false;
    }
    if (targetEnd > targetStart) {
        wrapSpace(wrap);
        if (!appendPlain(self, wrap, "(", 1u, TERMINAL_MARKDOWN_QUOTE, 0))
            return false;
        if (!appendPlain(self, wrap, text + targetStart, targetEnd - targetStart,
                         TERMINAL_MARKDOWN_QUOTE, 0))
            return false;
        if (!appendPlain(self, wrap, ")", 1u, TERMINAL_MARKDOWN_QUOTE, 0))
            return false;
    }
    return true;
}

static bool emitInline(TerminalMarkdownView *self, MarkdownWrap *wrap, const char *text,
                       size_t length, TerminalMarkdownRole base, uint8_t style, size_t depth) {
    if (depth > MARKDOWN_MAX_INLINE_DEPTH)
        return appendPlain(self, wrap, text, length, base, style);
    size_t at = 0;
    while (at < length) {
        char character = text[at];
        if (character == '\\' && at + 1u < length && isPunctuation(text[at + 1u])) {
            if (!appendPlain(self, wrap, text + at + 1u, 1u, base, style))
                return false;
            at += 2u;
            continue;
        }
        if (character == '`') {
            size_t run = 0;
            while (at + run < length && text[at + run] == '`')
                ++run;
            size_t close = findBacktickRun(text, length, at + run, run);
            if (close != MARKDOWN_NO_INDEX) {
                if (!wrapWord(self, wrap, text + at + run, close - at - run,
                              TERMINAL_MARKDOWN_CODE, 0))
                    return false;
                at = close + run;
                continue;
            }
        }
        if ((character == '*' || character == '_') && at + 1u < length
            && text[at + 1u] == character) {
            size_t close = findBytes(text, length, at + 2u, text + at, 2u);
            if (close != MARKDOWN_NO_INDEX) {
                if (!emitInline(self, wrap, text + at + 2u, close - at - 2u, base,
                                (uint8_t) (style | TERMINAL_STYLE_BOLD), depth + 1u))
                    return false;
                at = close + 2u;
                continue;
            }
        }
        if (character == '*' || character == '_') {
            size_t close = findBytes(text, length, at + 1u, text + at, 1u);
            if (close != MARKDOWN_NO_INDEX) {
                if (!emitInline(self, wrap, text + at + 1u, close - at - 1u, base,
                                (uint8_t) (style | TERMINAL_STYLE_UNDERLINE), depth + 1u))
                    return false;
                at = close + 1u;
                continue;
            }
        }
        if (character == '!' && at + 1u < length && text[at + 1u] == '[') {
            size_t close = findBytes(text, length, at + 2u, "]", 1u);
            if (close != MARKDOWN_NO_INDEX) {
                if (!appendPlain(self, wrap, MARKDOWN_IMAGE_GLYPH,
                                 strlen(MARKDOWN_IMAGE_GLYPH), TERMINAL_MARKDOWN_MARKER, 0))
                    return false;
                if (!appendPlain(self, wrap, text + at + 2u, close - at - 2u,
                                 TERMINAL_MARKDOWN_MARKER, 0))
                    return false;
                size_t after = findBytes(text, length, close, "](", 2u);
                size_t end = after == MARKDOWN_NO_INDEX
                           ? MARKDOWN_NO_INDEX : findBytes(text, length, after + 2u, ")", 1u);
                at = end == MARKDOWN_NO_INDEX ? close + 1u : end + 1u;
                continue;
            }
        }
        if (character == '[') {
            size_t close = findBytes(text, length, at + 1u, "]", 1u);
            if (close != MARKDOWN_NO_INDEX) {
                size_t paren = findBytes(text, length, close, "](", 2u);
                size_t end = paren == MARKDOWN_NO_INDEX
                           ? MARKDOWN_NO_INDEX : findBytes(text, length, paren + 2u, ")", 1u);
                if (paren != MARKDOWN_NO_INDEX && end != MARKDOWN_NO_INDEX) {
                    if (!emitLink(self, wrap, text, length, at, close, style, depth))
                        return false;
                    at = end + 1u;
                    continue;
                }
            }
        }
        if (character == '<') {
            size_t close = findBytes(text, length, at + 1u, ">", 1u);
            if (close != MARKDOWN_NO_INDEX) {
                const char *inner = text + at + 1u;
                size_t innerLength = close - at - 1u;
                if (innerLength >= 4u
                    && (strncasecmp(inner, "http", 4u) == 0
                        || strncasecmp(inner, "mailto", 6u) == 0)) {
                    if (!appendPlain(self, wrap, inner, innerLength, TERMINAL_MARKDOWN_LINK, style))
                        return false;
                }
                at = close + 1u;
                continue;
            }
        }
        size_t start = at;
        while (at < length) {
            char current = text[at];
            if (current == '\\' || current == '`' || current == '*' || current == '_'
                || current == '[' || current == '!' || current == '<')
                break;
            ++at;
        }
        if (at == start) {
            if (!appendPlain(self, wrap, text + at, 1u, base, style))
                return false;
            ++at;
        } else if (!appendPlain(self, wrap, text + start, at - start, base, style)) {
            return false;
        }
    }
    return true;
}

static size_t leadingSpaces(const char *text, size_t length) {
    size_t at = 0;
    while (at < length && text[at] == ' ')
        ++at;
    return at;
}

static bool lineBlank(const char *text, size_t length) {
    for (size_t at = 0; at < length; ++at)
        if (!isSpace(text[at]))
            return false;
    return true;
}

static size_t trimRight(const char *text, size_t length) {
    while (length > 0 && isSpace(text[length - 1u]))
        --length;
    return length;
}

static size_t headingLevel(const char *text, size_t length) {
    size_t spaces = leadingSpaces(text, length);
    if (spaces > 3u)
        return 0;
    size_t hashes = 0;
    while (spaces + hashes < length && text[spaces + hashes] == '#')
        ++hashes;
    if (hashes == 0 || hashes > 6u)
        return 0;
    if (spaces + hashes < length && text[spaces + hashes] != ' ')
        return 0;
    return hashes;
}

static size_t headingTextStart(const char *text, size_t length, size_t *outLength) {
    size_t spaces = leadingSpaces(text, length);
    size_t hashes = 0;
    while (spaces + hashes < length && text[spaces + hashes] == '#')
        ++hashes;
    size_t start = spaces + hashes;
    if (start < length && text[start] == ' ')
        ++start;
    size_t end = start;
    while (end < length)
        ++end;
    while (end > start && isSpace(text[end - 1u]))
        --end;
    while (end > start && text[end - 1u] == '#') {
        size_t candidate = end - 1u;
        while (candidate > start && text[candidate - 1u] == '#')
            --candidate;
        if (candidate == start || text[candidate - 1u] == ' ')
            end = candidate;
        else
            break;
    }
    while (end > start && isSpace(text[end - 1u]))
        --end;
    *outLength = end - start;
    return start;
}

static bool isFence(const char *text, size_t length, char *marker, size_t *run,
                    size_t *contentStart) {
    size_t spaces = leadingSpaces(text, length);
    if (spaces > 3u || spaces >= length)
        return false;
    char candidate = text[spaces];
    if (candidate != '`' && candidate != '~')
        return false;
    size_t count = 0;
    while (spaces + count < length && text[spaces + count] == candidate)
        ++count;
    if (count < 3u)
        return false;
    *marker = candidate;
    *run = count;
    *contentStart = spaces + count;
    return true;
}

static bool isClosingFence(const char *text, size_t length, char marker, size_t run) {
    size_t spaces = leadingSpaces(text, length);
    if (spaces > 3u || spaces >= length || text[spaces] != marker)
        return false;
    size_t count = 0;
    while (spaces + count < length && text[spaces + count] == marker)
        ++count;
    if (count < run)
        return false;
    for (size_t at = spaces + count; at < length; ++at)
        if (!isSpace(text[at]))
            return false;
    return true;
}

static bool isThematic(const char *text, size_t length) {
    char marker = 0;
    size_t count = 0;
    for (size_t at = 0; at < length; ++at) {
        char current = text[at];
        if (isSpace(current))
            continue;
        if (marker == 0)
            marker = current;
        else if (current != marker)
            return false;
        ++count;
    }
    return count >= 3u && (marker == '-' || marker == '*' || marker == '_');
}

static size_t setextLevel(const char *text, size_t length) {
    size_t spaces = leadingSpaces(text, length);
    if (spaces > 3u || spaces >= length)
        return 0;
    char marker = text[spaces];
    if (marker != '=' && marker != '-')
        return 0;
    size_t count = 0;
    size_t at = spaces;
    while (at < length && text[at] == marker) {
        ++at;
        ++count;
    }
    if (count == 0)
        return 0;
    for (; at < length; ++at)
        if (!isSpace(text[at]))
            return 0;
    return marker == '=' ? 1u : 2u;
}

static bool quoteLine(const char *text, size_t length, size_t *depth, size_t *contentStart) {
    size_t spaces = leadingSpaces(text, length);
    if (spaces > 3u || spaces >= length || text[spaces] != '>')
        return false;
    size_t at = spaces;
    size_t count = 0;
    while (at < length && text[at] == '>') {
        ++count;
        ++at;
        if (at < length && text[at] == ' ')
            ++at;
    }
    *depth = count;
    *contentStart = at;
    return true;
}

static MarkdownListMarker listMarker(const char *text, size_t length) {
    MarkdownListMarker marker = {0};
    size_t spaces = leadingSpaces(text, length);
    if (spaces >= length)
        return marker;
    size_t at = spaces;
    size_t number = 0;
    bool ordered = false;
    if (text[at] == '-' || text[at] == '*' || text[at] == '+') {
        ++at;
    } else if (text[at] >= '0' && text[at] <= '9') {
        size_t digits = 0;
        while (at < length && text[at] >= '0' && text[at] <= '9' && digits < 9u) {
            number = number * 10u + (size_t) (text[at] - '0');
            ++at;
            ++digits;
        }
        if (digits == 0 || at >= length || (text[at] != '.' && text[at] != ')'))
            return marker;
        ++at;
        ordered = true;
    } else {
        return marker;
    }
    if (at >= length || text[at] != ' ')
        return marker;
    ++at;
    if (at < length && text[at] == ' ')
        ++at;
    marker.valid = true;
    marker.ordered = ordered;
    marker.indent = spaces;
    marker.number = number;
    marker.contentStart = at;
    if (at + 3u < length && text[at] == '[' && text[at + 2u] == ']'
        && text[at + 3u] == ' ') {
        char state = text[at + 1u];
        if (state == ' ' || state == 'x' || state == 'X') {
            marker.task = true;
            marker.taskDone = state == 'x' || state == 'X';
            marker.contentStart = at + 4u;
        }
    }
    return marker;
}

static bool tableDelimiter(const char *text, size_t length, size_t *columns) {
    size_t count = 0;
    size_t at = 0;
    bool sawDash = false;
    if (length == 0)
        return false;
    while (at <= length) {
        size_t start = at;
        while (at < length && text[at] != '|')
            ++at;
        size_t cellLength = at - start;
        while (cellLength > 0 && isSpace(text[start]))
            ++start, --cellLength;
        while (cellLength > 0 && isSpace(text[start + cellLength - 1u]))
            --cellLength;
        if (cellLength == 0 && count == 0) {
            if (at < length) {
                ++at;
                continue;
            }
            return false;
        }
        size_t colons = 0;
        size_t dashes = 0;
        for (size_t index = 0; index < cellLength; ++index) {
            char current = text[start + index];
            if (current == ':')
                ++colons;
            else if (current == '-')
                ++dashes;
            else
                return false;
        }
        if (colons > 2u || dashes < 1u)
            return false;
        sawDash = true;
        ++count;
        if (at < length)
            ++at;
        else
            break;
    }
    *columns = count;
    return sawDash && count >= 2u;
}

static bool tableCandidate(const char *text, size_t length, const char *next, size_t nextLength,
                           size_t *columns) {
    if (findBytes(text, length, 0, "|", 1u) == MARKDOWN_NO_INDEX)
        return false;
    return tableDelimiter(next, nextLength, columns);
}

static bool emitGap(TerminalMarkdownView *self) {
    if ((*self).rowCount == 0)
        return true;
    MarkdownRow *last = &(*self).rows[(*self).rowCount - 1u];
    if ((*last).segmentCount == 0)
        return true;
    return appendRow(self);
}

static bool emitRule(TerminalMarkdownView *self, size_t width) {
    if (width == 0)
        return true;
    if (!appendRow(self))
        return false;
    return appendSegment(self, MARKDOWN_RULE_GLYPH, strlen(MARKDOWN_RULE_GLYPH), width,
                         TERMINAL_MARKDOWN_RULE, 0);
}

static bool emitHeading(TerminalMarkdownView *self, const char *text, size_t length, size_t level) {
    if (!emitGap(self))
        return false;
    MarkdownPrefix prefix = {0};
    MarkdownWrap wrap = { .prefix = &prefix, .width = (*self).width };
    size_t bodyStart = headingTextStart(text, length, &length);
    if (!emitInline(self, &wrap, text + bodyStart, length, TERMINAL_MARKDOWN_HEADING, 0, 0))
        return false;
    if (level == 1u && (*self).width != 0)
        return emitRule(self, (*self).width);
    return true;
}

static bool emitParagraphLine(TerminalMarkdownView *self, MarkdownWrap *wrap, const char *text,
                              size_t length) {
    size_t spaces = leadingSpaces(text, length);
    if (spaces < length && spaces <= 3u)
        return emitInline(self, wrap, text + spaces, length - spaces, TERMINAL_MARKDOWN_BODY, 0, 0);
    return emitInline(self, wrap, text, length, TERMINAL_MARKDOWN_BODY, 0, 0);
}

static bool emitFence(TerminalMarkdownView *self, const char *const *lines, size_t lineCount,
                      size_t *index, char marker, size_t run, size_t contentStart) {
    if (!emitGap(self))
        return false;
    MarkdownPrefix prefix = {0};
    prefixAdd(&prefix, MARKDOWN_QUOTE_GLYPH, strlen(MARKDOWN_QUOTE_GLYPH),
              TERMINAL_MARKDOWN_FENCE, 0);
    MarkdownWrap wrap = { .prefix = &prefix, .width = (*self).width };
    (void) contentStart;
    size_t at = *index + 1u;
    bool first = true;
    while (at < lineCount) {
        size_t length = strlen(lines[at]);
        if (isClosingFence(lines[at], length, marker, run)) {
            ++at;
            break;
        }
        if (!wrapStart(self, &wrap, first))
            return false;
        first = false;
        if (length != 0 && !wrapWord(self, &wrap, lines[at], length, TERMINAL_MARKDOWN_FENCE, 0))
            return false;
        ++at;
    }
    *index = at;
    return true;
}

static bool emitList(TerminalMarkdownView *self, const char *const *lines, size_t lineCount,
                     size_t *index) {
    if (!emitGap(self))
        return false;
    size_t at = *index;
    while (at < lineCount) {
        size_t length = strlen(lines[at]);
        MarkdownListMarker marker = listMarker(lines[at], length);
        if (!marker.valid)
            break;
        MarkdownPrefix prefix = {0};
        size_t indent = marker.indent;
        if (indent > (*self).width / 2u)
            indent = (*self).width / 2u;
        prefixSpaces(&prefix, indent, TERMINAL_MARKDOWN_BODY, 0);
        prefixBeginMarker(&prefix);
        char label[16];
        if (marker.task) {
            prefixAdd(&prefix, marker.taskDone ? "☑ " : "☐ ", 4u,
                      TERMINAL_MARKDOWN_MARKER, 0);
        } else if (marker.ordered) {
            int written = snprintf(label, sizeof label, "%zu. ", marker.number);
            if (written > 0)
                prefixAdd(&prefix, label, (size_t) written, TERMINAL_MARKDOWN_MARKER, 0);
        } else {
            prefixAdd(&prefix, "• ", 3u, TERMINAL_MARKDOWN_MARKER, 0);
        }
        prefixEndMarker(&prefix);
        MarkdownWrap wrap = { .prefix = &prefix, .width = (*self).width };
        size_t contentEnd = trimRight(lines[at], length);
        if (contentEnd > marker.contentStart) {
            if (!emitInline(self, &wrap, lines[at] + marker.contentStart,
                            contentEnd - marker.contentStart,
                            TERMINAL_MARKDOWN_BODY, 0, 0))
                return false;
        } else if (!wrapStart(self, &wrap, true)) {
            return false;
        }
        size_t next = at + 1u;
        while (next < lineCount) {
            size_t nextLength = strlen(lines[next]);
            if (lineBlank(lines[next], nextLength))
                break;
            MarkdownListMarker nested = listMarker(lines[next], nextLength);
            if (nested.valid)
                break;
            size_t spaces = leadingSpaces(lines[next], nextLength);
            if (spaces < marker.contentStart)
                break;
            wrapSpace(&wrap);
            size_t end = trimRight(lines[next], nextLength);
            if (!emitInline(self, &wrap, lines[next] + spaces, end - spaces,
                            TERMINAL_MARKDOWN_BODY, 0, 0))
                return false;
            ++next;
        }
        at = next;
        while (at < lineCount && lineBlank(lines[at], strlen(lines[at])))
            ++at;
        if (at < lineCount && !listMarker(lines[at], strlen(lines[at])).valid)
            break;
    }
    *index = at;
    return true;
}

static bool emitQuote(TerminalMarkdownView *self, const char *const *lines, size_t lineCount,
                      size_t *index) {
    if (!emitGap(self))
        return false;
    size_t at = *index;
    size_t depth = 0;
    size_t contentStart = 0;
    if (!quoteLine(lines[at], strlen(lines[at]), &depth, &contentStart))
        return false;
    MarkdownPrefix prefix = {0};
    prefixQuote(&prefix, depth);
    MarkdownWrap wrap = { .prefix = &prefix, .width = (*self).width };
    while (at < lineCount) {
        size_t length = strlen(lines[at]);
        size_t lineDepth = 0;
        size_t lineContent = 0;
        if (!quoteLine(lines[at], length, &lineDepth, &lineContent))
            break;
        if (lineDepth != depth)
            break;
        size_t end = trimRight(lines[at], length);
        if (end > lineContent) {
            wrapSpace(&wrap);
            if (!emitInline(self, &wrap, lines[at] + lineContent, end - lineContent,
                            TERMINAL_MARKDOWN_QUOTE, 0, 0))
                return false;
        } else if (!wrap.open) {
            if (!wrapStart(self, &wrap, !wrap.started))
                return false;
        } else if (!wrapBreak(self, &wrap)) {
            return false;
        }
        ++at;
    }
    *index = at;
    return true;
}

static void tableCells(const char *text, size_t length, const char **cells, size_t *lengths,
                       size_t *count) {
    size_t at = 0;
    size_t total = 0;
    while (at <= length && total < MARKDOWN_MAX_COLUMNS) {
        size_t start = at;
        while (at < length && text[at] != '|')
            ++at;
        size_t startTrim = start;
        size_t endTrim = at;
        while (startTrim < endTrim && isSpace(text[startTrim]))
            ++startTrim;
        while (endTrim > startTrim && isSpace(text[endTrim - 1u]))
            --endTrim;
        if (total == 0 && startTrim == endTrim && at < length) {
            ++at;
            continue;
        }
        cells[total] = text + startTrim;
        lengths[total] = endTrim - startTrim;
        ++total;
        if (at < length)
            ++at;
        else
            break;
    }
    *count = total;
}

static bool emitTable(TerminalMarkdownView *self, const char *const *lines, size_t lineCount,
                      size_t *index, size_t columns) {
    if (columns > MARKDOWN_MAX_COLUMNS)
        columns = MARKDOWN_MAX_COLUMNS;
    size_t widths[MARKDOWN_MAX_COLUMNS] = {0};
    size_t rowStarts[MARKDOWN_MAX_COLUMNS];
    size_t rowCount = 0;
    size_t at = *index;
    const char *cells[MARKDOWN_MAX_COLUMNS];
    size_t lengths[MARKDOWN_MAX_COLUMNS];
    size_t count = 0;
    tableCells(lines[at], strlen(lines[at]), cells, lengths, &count);
    for (size_t column = 0; column < columns && column < count; ++column) {
        size_t width = cellsOf(cells[column], lengths[column]);
        if (width > widths[column])
            widths[column] = width;
    }
    rowStarts[rowCount++] = at;
    at += 2u;
    while (at < lineCount) {
        size_t length = strlen(lines[at]);
        if (lineBlank(lines[at], length) || findBytes(lines[at], length, 0, "|", 1u) == MARKDOWN_NO_INDEX)
            break;
        tableCells(lines[at], length, cells, lengths, &count);
        for (size_t column = 0; column < columns && column < count; ++column) {
            size_t width = cellsOf(cells[column], lengths[column]);
            if (width > widths[column])
                widths[column] = width;
        }
        if (rowCount < MARKDOWN_MAX_COLUMNS)
            rowStarts[rowCount++] = at;
        ++at;
    }
    for (size_t column = 0; column < columns; ++column)
        if (widths[column] > MARKDOWN_MAX_COLUMN_CELLS)
            widths[column] = MARKDOWN_MAX_COLUMN_CELLS;
    size_t total = columns > 1u ? (columns - 1u) * 2u : 0u;
    for (size_t column = 0; column < columns; ++column)
        total += widths[column];
    while (total > (*self).width && columns != 0) {
        size_t widest = 0;
        for (size_t column = 1; column < columns; ++column)
            if (widths[column] > widths[widest])
                widest = column;
        if (widths[widest] <= 3u)
            break;
        --widths[widest];
        --total;
    }
    if (!emitGap(self))
        return false;
    for (size_t row = 0; row < rowCount; ++row) {
        size_t line = rowStarts[row];
        tableCells(lines[line], strlen(lines[line]), cells, lengths, &count);
        if (!appendRow(self))
            return false;
        for (size_t column = 0; column < columns; ++column) {
            const char *cell = column < count ? cells[column] : "";
            size_t cellLength = column < count ? lengths[column] : 0u;
            size_t available = widths[column];
            size_t used = 0;
            size_t take = bytesForCells(cell, cellLength, available, &used);
            if (take < cellLength && available > 1u) {
                size_t keep = bytesForCells(cell, cellLength, available - 1u, &used);
                if (!appendSegment(self, cell, keep, 1u,
                                   row == 0 ? TERMINAL_MARKDOWN_HEADING : TERMINAL_MARKDOWN_BODY, 0))
                    return false;
                if (!appendSegment(self, "…", 3u, 1u,
                                   row == 0 ? TERMINAL_MARKDOWN_HEADING : TERMINAL_MARKDOWN_BODY, 0))
                    return false;
                used += 1u;
            } else if (!appendSegment(self, cell, take, 1u,
                                      row == 0 ? TERMINAL_MARKDOWN_HEADING : TERMINAL_MARKDOWN_BODY, 0)) {
                return false;
            }
            if (used < available) {
                if (!appendSegment(self, MARKDOWN_SPACES, available - used, 1u,
                                   TERMINAL_MARKDOWN_BODY, 0))
                    return false;
            }
            if (column + 1u < columns) {
                if (!appendSegment(self, "  ", 2u, 1u, TERMINAL_MARKDOWN_BODY, 0))
                    return false;
            }
        }
        if (row == 0) {
            if (!appendRow(self))
                return false;
            if (!appendSegment(self, MARKDOWN_RULE_GLYPH, strlen(MARKDOWN_RULE_GLYPH),
                               total, TERMINAL_MARKDOWN_RULE, 0))
                return false;
        }
    }
    *index = at;
    return true;
}

static bool startsBlock(const char *text, size_t length) {
    if (lineBlank(text, length))
        return true;
    if (headingLevel(text, length) != 0)
        return true;
    char marker = 0;
    size_t run = 0;
    size_t contentStart = 0;
    if (isFence(text, length, &marker, &run, &contentStart))
        return true;
    if (isThematic(text, length))
        return true;
    if (listMarker(text, length).valid)
        return true;
    size_t depth = 0;
    if (quoteLine(text, length, &depth, &contentStart))
        return true;
    return false;
}

static bool layout(TerminalMarkdownView *self) {
    size_t index = 0;
    while (index < (*self).lineCount && !(*self).failed && !(*self).truncated) {
        const char *line = (*self).lines[index];
        size_t length = strlen(line);
        if (lineBlank(line, length)) {
            ++index;
            continue;
        }
        char fenceMarker = 0;
        size_t fenceRun = 0;
        size_t fenceContent = 0;
        if (isFence(line, length, &fenceMarker, &fenceRun, &fenceContent)) {
            if (!emitFence(self, (*self).lines, (*self).lineCount, &index,
                           fenceMarker, fenceRun, fenceContent))
                break;
            continue;
        }
        size_t level = headingLevel(line, length);
        if (level != 0) {
            if (!emitHeading(self, line, length, level))
                break;
            ++index;
            continue;
        }
        if (index + 1u < (*self).lineCount) {
            size_t nextLength = strlen((*self).lines[index + 1u]);
            size_t setext = setextLevel((*self).lines[index + 1u], nextLength);
            if (setext != 0 && !startsBlock(line, length)) {
                if (!emitHeading(self, line, length, setext))
                    break;
                index += 2u;
                continue;
            }
        }
        if (isThematic(line, length)) {
            if (!emitGap(self))
                break;
            if (!emitRule(self, (*self).width))
                break;
            ++index;
            continue;
        }
        if (listMarker(line, length).valid) {
            if (!emitList(self, (*self).lines, (*self).lineCount, &index))
                break;
            continue;
        }
        size_t quoteDepth = 0;
        size_t quoteContent = 0;
        if (quoteLine(line, length, &quoteDepth, &quoteContent)) {
            if (!emitQuote(self, (*self).lines, (*self).lineCount, &index))
                break;
            continue;
        }
        if (index + 1u < (*self).lineCount) {
            size_t nextLength = strlen((*self).lines[index + 1u]);
            size_t columns = 0;
            if (tableCandidate(line, length, (*self).lines[index + 1u], nextLength, &columns)) {
                if (!emitTable(self, (*self).lines, (*self).lineCount, &index, columns))
                    break;
                continue;
            }
        }
        if (!emitGap(self))
            break;
        MarkdownPrefix prefix = {0};
        MarkdownWrap wrap = { .prefix = &prefix, .width = (*self).width };
        bool firstLine = true;
        while (index < (*self).lineCount) {
            const char *paragraph = (*self).lines[index];
            size_t paragraphLength = strlen(paragraph);
            if (lineBlank(paragraph, paragraphLength))
                break;
            if (!firstLine && startsBlock(paragraph, paragraphLength))
                break;
            if (firstLine && startsBlock(paragraph, paragraphLength))
                break;
            size_t rightTrim = trimRight(paragraph, paragraphLength);
            bool hardBreak = rightTrim != paragraphLength
                          || (rightTrim > 0 && paragraph[rightTrim - 1u] == '\\');
            if (hardBreak && rightTrim > 0 && paragraph[rightTrim - 1u] == '\\')
                --rightTrim;
            if (!firstLine) {
                if (hardBreak) {
                    if (!wrapBreak(self, &wrap))
                        break;
                } else {
                    wrapSpace(&wrap);
                }
            }
            if (!emitParagraphLine(self, &wrap, paragraph, rightTrim))
                break;
            if (hardBreak && !wrapBreak(self, &wrap))
                break;
            firstLine = false;
            ++index;
        }
    }
    if ((*self).truncated && (*self).rowCount + 1u < TERMINAL_MARKDOWN_ROWS_LIMIT) {
        if (!appendRowInternal(self, true))
            return false;
        if (!appendSegment(self, "… preview truncated", strlen("… preview truncated"), 1u,
                           TERMINAL_MARKDOWN_MARKER, 0))
            return false;
    }
    return !(*self).failed;
}

TerminalMarkdownView *TerminalMarkdownView_0(void) {
    TerminalMarkdownView *self = Memory_alloc(TYPE_BYTE_ARRAY, sizeof(TerminalMarkdownView));
    if (self == nullptr)
        return nullptr;
    *self = (TerminalMarkdownView) {0};
    (*self).style.body = TerminalPen_0();
    (*self).style.heading = TerminalPen_1(TERMINAL_STYLE_BOLD);
    (*self).style.code = TerminalPen_1(TERMINAL_STYLE_REVERSE);
    (*self).style.link = TerminalPen_1(TERMINAL_STYLE_UNDERLINE);
    (*self).style.quote = TerminalPen_1(TERMINAL_STYLE_DIM);
    (*self).style.rule = TerminalPen_1(TERMINAL_STYLE_DIM);
    (*self).style.marker = TerminalPen_1(TERMINAL_STYLE_BOLD);
    (*self).style.fence = TerminalPen_1(TERMINAL_STYLE_DIM);
    return self;
}

void TerminalMarkdownView_free(TerminalMarkdownView *self) {
    if (self == nullptr)
        return;
    Memory_free((*self).rows);
    Memory_free((*self).segments);
    Memory_free(self);
}

bool TerminalMarkdownView_setSource(TerminalMarkdownView *self, const char *const *lines,
                                    size_t count) {
    if (self == nullptr) {
        THROW("TerminalMarkdownView source: null view");
        return false;
    }
    if (count != 0 && lines == nullptr) {
        THROW("TerminalMarkdownView source: null list");
        return false;
    }
    for (size_t index = 0; index < count; ++index)
        if (lines[index] == nullptr) {
            THROW("TerminalMarkdownView source: null line at %zu", index);
            return false;
        }
    (*self).lines = lines;
    (*self).lineCount = count;
    (*self).dirty = true;
    (*self).ready = false;
    (*self).truncated = false;
    (*self).failed = false;
    (*self).rowCount = 0;
    (*self).segmentCount = 0;
    (*self).scroll = 0;
    return true;
}

bool TerminalMarkdownView_setViewport(TerminalMarkdownView *self, size_t width) {
    if (self == nullptr) {
        THROW("TerminalMarkdownView viewport: null view");
        return false;
    }
    if (width == 0) {
        THROW("TerminalMarkdownView viewport: zero width");
        return false;
    }
    if ((*self).ready && !(*self).dirty && (*self).width == width)
        return true;
    (*self).width = width;
    (*self).dirty = false;
    (*self).ready = false;
    (*self).truncated = false;
    (*self).failed = false;
    (*self).rowCount = 0;
    (*self).segmentCount = 0;
    if ((*self).lines == nullptr || (*self).lineCount == 0) {
        (*self).ready = true;
        (*self).scroll = 0;
        return true;
    }
    if (!layout(self)) {
        (*self).ready = false;
        return false;
    }
    (*self).ready = true;
    if ((*self).scroll > TerminalMarkdownView_maxScroll(self))
        (*self).scroll = TerminalMarkdownView_maxScroll(self);
    return true;
}

size_t TerminalMarkdownView_rowCount(const TerminalMarkdownView *self) {
    return self != nullptr ? (*self).rowCount : 0u;
}

size_t TerminalMarkdownView_getWidth(const TerminalMarkdownView *self) {
    return self != nullptr ? (*self).width : 0u;
}

bool TerminalMarkdownView_setVisibleRows(TerminalMarkdownView *self, size_t rows) {
    if (self == nullptr) {
        THROW("TerminalMarkdownView visible rows: null view");
        return false;
    }
    (*self).visibleRows = rows;
    if ((*self).scroll > TerminalMarkdownView_maxScroll(self))
        (*self).scroll = TerminalMarkdownView_maxScroll(self);
    return true;
}

size_t TerminalMarkdownView_visibleRows(const TerminalMarkdownView *self) {
    return self != nullptr ? (*self).visibleRows : 0u;
}

size_t TerminalMarkdownView_maxScroll(const TerminalMarkdownView *self) {
    if (self == nullptr)
        return 0u;
    if ((*self).rowCount <= (*self).visibleRows)
        return 0u;
    return (*self).rowCount - (*self).visibleRows;
}

bool TerminalMarkdownView_setScroll(TerminalMarkdownView *self, size_t row) {
    if (self == nullptr) {
        THROW("TerminalMarkdownView scroll: null view");
        return false;
    }
    size_t maximum = TerminalMarkdownView_maxScroll(self);
    (*self).scroll = row > maximum ? maximum : row;
    return true;
}

size_t TerminalMarkdownView_getScroll(const TerminalMarkdownView *self) {
    return self != nullptr ? (*self).scroll : 0u;
}

bool TerminalMarkdownView_scrollBy(TerminalMarkdownView *self, long delta) {
    if (self == nullptr)
        return false;
    size_t maximum = TerminalMarkdownView_maxScroll(self);
    size_t wanted = (*self).scroll;
    if (delta < 0) {
        size_t amount = (size_t) -delta;
        wanted = amount > wanted ? 0u : wanted - amount;
    } else if (delta > 0) {
        wanted += (size_t) delta;
        if (wanted > maximum)
            wanted = maximum;
    }
    if (wanted == (*self).scroll)
        return false;
    (*self).scroll = wanted;
    return true;
}

bool TerminalMarkdownView_isReady(const TerminalMarkdownView *self) {
    return self != nullptr && (*self).ready;
}

bool TerminalMarkdownView_isTruncated(const TerminalMarkdownView *self) {
    return self != nullptr && (*self).truncated;
}

size_t TerminalMarkdownView_exhaustionCount(const TerminalMarkdownView *self) {
    return self != nullptr ? (*self).refusals : 0u;
}

void TerminalMarkdownView_clearExhaustion(TerminalMarkdownView *self) {
    if (self != nullptr)
        (*self).refusals = 0;
}

bool TerminalMarkdownView_setStyle(TerminalMarkdownView *self, TerminalMarkdownStyle style) {
    if (self == nullptr) {
        THROW("TerminalMarkdownView style: null view");
        return false;
    }
    (*self).style = style;
    return true;
}

TerminalMarkdownStyle TerminalMarkdownView_getStyle(const TerminalMarkdownView *self) {
    return self != nullptr ? (*self).style : (TerminalMarkdownStyle) {0};
}

static TerminalPen penForRole(const TerminalMarkdownStyle *style, TerminalMarkdownRole role) {
    switch (role) {
        case TERMINAL_MARKDOWN_HEADING: return (*style).heading;
        case TERMINAL_MARKDOWN_CODE: return (*style).code;
        case TERMINAL_MARKDOWN_LINK: return (*style).link;
        case TERMINAL_MARKDOWN_QUOTE: return (*style).quote;
        case TERMINAL_MARKDOWN_RULE: return (*style).rule;
        case TERMINAL_MARKDOWN_MARKER: return (*style).marker;
        case TERMINAL_MARKDOWN_FENCE: return (*style).fence;
        default: return (*style).body;
    }
}

void TerminalMarkdownView_render(const TerminalMarkdownView *self, TerminalCanvas *canvas) {
    if (self == nullptr || canvas == nullptr || !TerminalCanvas_isValid(canvas))
        return;
    if (!(*self).ready)
        return;
    size_t visible = TerminalCanvas_getHeight(canvas);
    size_t width = TerminalCanvas_getWidth(canvas);
    for (size_t row = 0; row < visible && (*self).scroll + row < (*self).rowCount; ++row) {
        const MarkdownRow *record = &(*self).rows[(*self).scroll + row];
        size_t x = 0;
        for (size_t index = 0; index < (*record).segmentCount; ++index) {
            const MarkdownSegment *segment = &(*self).segments[(*record).firstSegment + index];
            TerminalPen pen = penForRole(&(*self).style, (*segment).role);
            pen.style = (uint8_t) (pen.style | (*segment).style);
            size_t advance = cellsOf((*segment).text, (*segment).length);
            for (size_t repeat = 0; repeat < (*segment).repeat; ++repeat) {
                if (x >= width)
                    break;
                TerminalCanvas_text(canvas, x, row, (*segment).text, (*segment).length, pen);
                if (advance == 0)
                    break;
                x += advance;
            }
            if (x >= width)
                break;
        }
    }
}

TerminalProjection TerminalMarkdownView_toString(const TerminalMarkdownView *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalMarkdownView{rows=%zu,width=%zu,scroll=%zu,ready=%d,truncated=%d}",
        (*self).rowCount, (*self).width, (*self).scroll, (*self).ready, (*self).truncated);
}

TerminalProjection TerminalMarkdownView_toStringStruct(const TerminalMarkdownView *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalMarkdownView{lines=<borrowed:%zu>,lineCount=%zu,dirty=%d,ready=%d,"
        "truncated=%d,failed=%d,width=%zu,visibleRows=%zu,scroll=%zu,rows=<owned:%zu>,"
        "rowCount=%zu,rowCapacity=%zu,segments=<owned:%zu>,segmentCount=%zu,"
        "segmentCapacity=%zu,style=<pens>,refusals=%zu}",
        (*self).lineCount, (*self).lineCount, (*self).dirty, (*self).ready,
        (*self).truncated, (*self).failed, (*self).width, (*self).visibleRows,
        (*self).scroll, (*self).rowCapacity, (*self).rowCount, (*self).rowCapacity,
        (*self).segmentCapacity, (*self).segmentCount, (*self).segmentCapacity,
        (*self).refusals);
}
