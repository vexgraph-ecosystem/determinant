#include "ui/markdown/terminal_markdown_view.h"
#include "ui/markdown/markdown_text.h"
#include "ui/markdown/markdown_inline.h"
#include "ui/markdown/markdown_table.h"
#include "format/terminal_style.h"
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
 * underlined without printing its asterisks, inline code becomes green text without
 * a reverse-video box, lists become hanging indents, callouts become colored
 * sections marked by a bold left wall repeated on every wrapped row, and fences
 * become full-width translucent grey bands with no box glyphs. Tables remain
 * bordered. A code block is surrounded by one blank row above and below, and the
 * same band is used when the fence sits inside a callout or blockquote.
 * A callout has a separate, highlighted `! TYPE !` label row and an unbolded body.
 * The syntax markers themselves are
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
 * links or footnotes, do math, nest tables, or shape grapheme clusters. Table
 * columns prefer their measured natural widths and wrap at word boundaries only
 * when the whole table cannot fit the viewport; then available cells are divided
 * in proportion to each column's content need. If even the longest-word minimums
 * cannot fit, rows fall back to a vertically stacked label/value presentation
 * rather than clipping source text.
 * Ordered-list marker width comes from the full source span, with no fixed digit
 * count; siblings align to the widest marker. Emoji and other wide scalars consume
 * two terminal cells. Those are
 * stated limits, not accidents: the renderer must never pretend to a conformance
 * it does not have.
 *
 * Borrowing is explicit. The source lines belong to the caller; the view stores
 * only the pointer array and every span points into it. The caller clears the
 * source before freeing it. The style record is borrowed by value and resolved at
 * render time, so recolouring does not reflow. */

;;OVERVIEW
/** ============================================================================
 * CLASS: TerminalMarkdownView (ui/markdown/terminal_markdown_view.c)
 * STRUCT FIELDS (Mirroring ui/markdown/terminal_markdown_view.h)
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
 *   MarkdownCodeBlockRecord *codeBlocks  owned laid-out fence bookkeeping
 *   size_t codeBlockCount      fences in the current layout
 *   size_t codeBlockCapacity   admitted fence slots
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
 *   bool codeBlock             this row is part of a shaded fenced code block
 * SLOT RECORD: MarkdownCodeBlockRecord
 *   size_t firstRow            first shaded document row
 *   size_t rowCount            shaded rows in this block
 *   size_t firstSourceLine     first content line in the borrowed source
 *   size_t sourceLineCount     content lines (fences excluded)
 *   size_t quoteDepth          >0 when the fence sits inside a quote/callout
 * PRIVATE HELPERS
 *   MarkdownPrefixItem  const char *text (borrowed glyph span); size_t length
 *     (span bytes); size_t cells (display width); TerminalMarkdownRole role
 *     (pen role); uint8_t style (extra style flags).
 *   MarkdownPrefix  MarkdownPrefixItem items[16] (row prefix); size_t count
 *     (live prefix entries); size_t markerStart (continuation boundary);
 *     size_t markerCount (marked entries); size_t cells (prefix width);
 *     size_t hang (continuation indent); bool repeatOnWrap (repeat quote gutter).
 *   MarkdownListMarker  bool valid/ordered/task/taskDone; size_t indent,
 *     contentStart, markerLength (admitted marker and content offset).
 *   MarkdownCellLine  const char *text (borrowed cell slice); size_t length
 *     (slice bytes); size_t next (next source byte offset); size_t cells
 *     (display width of the slice).
 *   cellsOf/bytesForCells(text, length, ...)  cell-aware UTF-8 measurement
 *   refuse(self, reason)      count a cold refusal, reporting once per epoch
 *   growRows/growSegments/growCodeBlocks(self)  cold doubling growth preserving live entries
 *   appendRow/appendRowForced(self)  begin a wrapped row (the notice may bypass
 *                             the document limit by one)
 *   appendSegment(self, ...)  append to the open row
 *   appendCodeBlock/markCodeRows(self, ...)  record a fence and shade its rows
 *   fenceContentText(line, depth, ...)  strip quote markers from a copy line
 *   prefixSpaces/prefixQuote/prefixBeginMarker/prefixEndMarker
 *   wrapStart/wrapSpace/wrapWord/wrapBreak
 *   findBytes/findBacktickRun/emitInline
 *   emitGap/emitHeading/emitParagraphLine/emitFence/emitQuoteFence/emitRule
 *   emitList/emitQuote
 *   tableCellLine/tableCellLineCount/tableColumnMinimum/tableAllocateWidths
 *   emitTableBorder/appendTableSpaces/emitTableRow/emitTableCompact/emitTable
 *   /splitTableRow/trimSpan;
 *   tables prefer each column's measured content width and proportionally
 *   allocate viewport cells when the natural table does not fit; ordered list siblings share
 *   the widest marker column so digit-count changes do not shift item text;
 *   callouts use the > [!TYPE] form, with a bold yellow `! TYPE !` label row,
 *   a blank gutter row, and body-role content that does not inherit quote bold;
 *   a fenced block is a borderless full-width band painted under its text, with
 *   blank rows around it; a fence inside a quote/callout keeps the quote gutter
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
 *              size_t codeBlockCount(self)
 *              TerminalMarkdownCodeBlock codeBlockAt(self, row)
 *              TerminalMarkdownCodeLine codeBlockLine(self, block, line)
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
#define MARKDOWN_MAX_INLINE_DEPTH 8u
#define MARKDOWN_NO_INDEX ((size_t) -1)
// The code-block wash: a neutral grey at 40% alpha. It is composited over the
// host's own backdrop by the device backend, so one value reads on a dark and on a
// light theme. This is PRESENTATION, not content — nothing in the syntax depends
// on it, and a caller can replace the fence pen's background through setStyle.
#define MARKDOWN_CODE_BLOCK_BG TERMINAL_RGBA(0xA0, 0xA0, 0xA0, 0x44)
#define MARKDOWN_CODE_BLOCK_PAD 2u

static const char *const MARKDOWN_SPACES =
    "                                ";
static const char *const MARKDOWN_RULE_GLYPH = "─";
static const char *const MARKDOWN_QUOTE_GLYPH = "| ";
static const char *const MARKDOWN_BORDER_TOP = "┌";
static const char *const MARKDOWN_BORDER_MIDDLE = "├";
static const char *const MARKDOWN_BORDER_BOTTOM = "└";
static const char *const MARKDOWN_BORDER_TOP_JOIN = "┬";
static const char *const MARKDOWN_BORDER_MIDDLE_JOIN = "┼";
static const char *const MARKDOWN_BORDER_BOTTOM_JOIN = "┴";
static const char *const MARKDOWN_BORDER_RIGHT = "┐";
static const char *const MARKDOWN_BORDER_RIGHT_MIDDLE = "┤";
static const char *const MARKDOWN_BORDER_RIGHT_BOTTOM = "┘";
static const char *const MARKDOWN_BORDER_HORIZONTAL = "─";
static const char *const MARKDOWN_BORDER_VERTICAL = "│";

typedef struct MarkdownSegment {
    const char *text;
    size_t length;
    size_t repeat;
    TerminalMarkdownRole role;
    uint16_t style;
    uint8_t variant;        // heading level (1..6) or callout kind; else 0
    const char *link;       // borrowed target, or nullptr for plain text
    size_t linkLength;
} MarkdownSegment;

typedef struct MarkdownRow {
    size_t firstSegment;
    size_t segmentCount;
    bool codeBlock;   // this row is part of a shaded fenced code block
} MarkdownRow;

typedef struct MarkdownPrefixItem {
    const char *text;
    size_t length;
    size_t cells;
    TerminalMarkdownRole role;
    uint16_t style;
    uint8_t variant;        // heading level or callout kind; else 0
} MarkdownPrefixItem;

typedef struct MarkdownPrefix {
    MarkdownPrefixItem items[MARKDOWN_PREFIX_ITEMS];
    size_t count;
    size_t markerStart;
    size_t markerCount;
    size_t cells;
    size_t hang;
    bool repeatOnWrap;
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
    size_t markerLength;
} MarkdownListMarker;

typedef TerminalMarkdownTableCellLine MarkdownCellLine;

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
    TerminalMarkdownCodeBlockPool *codeBlocks;
    TerminalMarkdownTheme theme;
    TerminalMarkdownStyle style;
    size_t hoverRow;
    size_t hoverColumn;
    size_t copiedBlock;
    size_t refusals;
};

static size_t cellsOf(const char *text, size_t length) {
    return TerminalMarkdownText_cells(text, length);
}

// Bytes that fit in `cells` CELLS, never splitting a UTF-8 scalar. A combining
// mark has zero width and rides with the scalar before it, so a row cannot start
// with a floating accent.
static size_t bytesForCells(const char *text, size_t length, size_t cells, size_t *usedCells) {
    return TerminalMarkdownText_bytesForCells(text, length, cells, usedCells);
}

static size_t scalarBytes(const char *text, size_t length) {
    return TerminalMarkdownText_scalarBytes(text, length);
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

// Mark every row in [first, last) as a shaded code row. Wrapping can append rows
// inside a single logical line, so the range is marked once the fence is complete
// instead of trying to tag each row at creation.
static void markCodeRows(TerminalMarkdownView *self, size_t first) {
    for (size_t index = first; index < (*self).rowCount; ++index)
        (*self).rows[index].codeBlock = true;
}

static bool appendRowInternal(TerminalMarkdownView *self, bool force) {
    if (!force && (*self).rowCount + 1u >= TERMINAL_MARKDOWN_ROWS_LIMIT) {
        (*self).truncated = true;
        return false;
    }
    if ((*self).rowCount == (*self).rowCapacity && !growRows(self))
        return false;
    (*self).rows[(*self).rowCount] = (MarkdownRow) { (*self).segmentCount, 0, false };
    ++(*self).rowCount;
    return true;
}

static bool appendRow(TerminalMarkdownView *self) {
    return appendRowInternal(self, false);
}

static bool appendSegment(TerminalMarkdownView *self, const char *text, size_t length,
                          size_t repeat, TerminalMarkdownRole role, uint16_t style) {
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
    (*segment).variant = 0;
    (*segment).link = nullptr;
    (*segment).linkLength = 0;
    ++(*self).segmentCount;
    ++(*row).segmentCount;
    return true;
}

// Stamp every segment appended since `first` with the structure the row belongs
// to: the heading level or callout kind, and the borrowed link target (if any).
// Wrap can split one span across rows, so the range — not the last segment — is
// what carries the identity.
static bool stampRange(TerminalMarkdownView *self, size_t first, uint8_t variant,
                       const char *link, size_t linkLength) {
    if (self == nullptr || (variant == 0 && link == nullptr))
        return true;
    for (size_t index = first; index < (*self).segmentCount; ++index) {
        MarkdownSegment *segment = &(*self).segments[index];
        (*segment).variant = variant;
        (*segment).link = link;
        (*segment).linkLength = linkLength;
    }
    return true;
}

static void prefixAddVariant(MarkdownPrefix *prefix, const char *text, size_t length,
                             TerminalMarkdownRole role, uint16_t style, uint8_t variant) {
    if (prefix == nullptr || (*prefix).count >= MARKDOWN_PREFIX_ITEMS || text == nullptr)
        return;
    MarkdownPrefixItem *item = &(*prefix).items[(*prefix).count];
    (*item).text = text;
    (*item).length = length;
    (*item).cells = cellsOf(text, length);
    (*item).role = role;
    (*item).style = style;
    (*item).variant = variant;
    (*prefix).cells += (*item).cells;
    ++(*prefix).count;
}

static void prefixAdd(MarkdownPrefix *prefix, const char *text, size_t length,
                      TerminalMarkdownRole role, uint16_t style) {
    prefixAddVariant(prefix, text, length, role, style, 0);
}

static void prefixSpaces(MarkdownPrefix *prefix, size_t cells,
                         TerminalMarkdownRole role, uint16_t style) {
    while (cells > 0) {
        size_t take = cells > MARKDOWN_SPACES_CAP ? MARKDOWN_SPACES_CAP : cells;
        prefixAdd(prefix, MARKDOWN_SPACES, take, role, style);
        cells -= take;
    }
}

static void prefixQuote(MarkdownPrefix *prefix, size_t depth) {
    for (size_t level = 0; level < depth && (*prefix).count < MARKDOWN_PREFIX_ITEMS; ++level)
        prefixAdd(prefix, MARKDOWN_QUOTE_GLYPH, strlen(MARKDOWN_QUOTE_GLYPH),
                  TERMINAL_MARKDOWN_QUOTE, TERMINAL_STYLE_BOLD);
    (*prefix).repeatOnWrap = true;
}

static void prefixBeginMarker(MarkdownPrefix *prefix) {
    (*prefix).markerStart = (*prefix).count;
    (*prefix).markerCount = 0;
    (*prefix).hang = 0;
}

static void prefixEndMarker(MarkdownPrefix *prefix) {
    if ((*prefix).count < (*prefix).markerStart)
        (*prefix).count = (*prefix).markerStart;
    (*prefix).markerCount = (*prefix).count - (*prefix).markerStart;
    for (size_t index = (*prefix).markerStart; index < (*prefix).count; ++index)
        (*prefix).hang += (*prefix).items[index].cells;
}

static bool wrapStart(TerminalMarkdownView *self, MarkdownWrap *wrap, bool first) {
    if (!appendRow(self))
        return false;
    (*wrap).open = true;
    (*wrap).x = 0;
    (*wrap).content = false;
    (*wrap).pendingSpace = false;
    const MarkdownPrefix *prefix = (*wrap).prefix;
    if (prefix != nullptr) {
        size_t limit = first || (*prefix).repeatOnWrap
            ? (*prefix).count : (*prefix).markerStart;
        for (size_t index = 0; index < limit; ++index) {
            const MarkdownPrefixItem *item = &(*prefix).items[index];
            size_t first = (*self).segmentCount;
            if (!appendSegment(self, (*item).text, (*item).length, 1u, (*item).role, (*item).style))
                return false;
            stampRange(self, first, (*item).variant, nullptr, 0);
            (*wrap).x += (*item).cells;
        }
        if (!first && !(*prefix).repeatOnWrap && (*prefix).hang != 0) {
            if (!appendSegment(self, MARKDOWN_SPACES, (*prefix).hang, 1u,
                               TERMINAL_MARKDOWN_BODY, 0))
                return false;
            (*wrap).x += (*prefix).hang;
        }
    }
    (*wrap).prefixCells = (*wrap).x;
    (*wrap).started = true;
    return true;
}

static void wrapSpace(MarkdownWrap *wrap) {
    if (wrap != nullptr)
        (*wrap).pendingSpace = true;
}

static bool wrapWord(TerminalMarkdownView *self, MarkdownWrap *wrap, const char *text,
                     size_t length, TerminalMarkdownRole role, uint16_t style) {
    if (wrap == nullptr || text == nullptr || length == 0)
        return true;
    if (!(*wrap).open && !wrapStart(self, wrap, !(*wrap).started))
        return false;
    size_t wordCells = cellsOf(text, length);
    size_t gap = (*wrap).pendingSpace && (*wrap).content ? 1u : 0u;
    if ((*wrap).content && (*wrap).x + gap + wordCells > (*wrap).width) {
        if (!wrapStart(self, wrap, false))
            return false;
        gap = 0;
    }
    if (gap != 0) {
        // The gap belongs to the span that follows it, so a link label is one
        // hover target and a heading's inter-word space keeps the heading pen.
        if (!appendSegment(self, " ", 1u, 1u, role, 0))
            return false;
        ++(*wrap).x;
    }
    (*wrap).pendingSpace = false;
    size_t available = (*wrap).width > (*wrap).x ? (*wrap).width - (*wrap).x : 0u;
    if (wordCells <= available) {
        if (!appendSegment(self, text, length, 1u, role, style))
            return false;
        (*wrap).x += wordCells;
        (*wrap).content = true;
        return true;
    }
    size_t at = 0;
    while (at < length) {
        available = (*wrap).width > (*wrap).x ? (*wrap).width - (*wrap).x : 0u;
        if (available == 0) {
            if (!wrapStart(self, wrap, false))
                return false;
            available = (*wrap).width > (*wrap).x ? (*wrap).width - (*wrap).x : 1u;
        }
        size_t used = 0;
        size_t take = bytesForCells(text + at, length - at, available, &used);
        if (take == 0) {
            take = scalarBytes(text + at, length - at);
            used = cellsOf(text + at, take);
        }
        if (!appendSegment(self, text + at, take, 1u, role, style))
            return false;
        (*wrap).x += used;
        (*wrap).content = true;
        at += take;
        if (at < length && !wrapStart(self, wrap, false))
            return false;
    }
    return true;
}

static bool wrapBreak(TerminalMarkdownView *self, MarkdownWrap *wrap) {
    if (wrap == nullptr || !(*wrap).open || !(*wrap).content)
        return true;
    (*wrap).pendingSpace = false;
    return wrapStart(self, wrap, false);
}

static size_t findBytes(const char *text, size_t length, size_t from,
                        const char *needle, size_t needleLength) {
    return TerminalMarkdownText_find(text, length, from, needle, needleLength);
}

static bool isSpace(char character) {
    return character == ' ' || character == '\t';
}

static bool appendPlain(TerminalMarkdownView *self, MarkdownWrap *wrap, const char *text,
                        size_t length, TerminalMarkdownRole role, uint16_t style) {
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

// Place ONE parsed span: wrap it as a word (which can split it across rows), then
// give every segment it produced the span's link and the row's variant.
static bool wrapSpan(TerminalMarkdownView *self, MarkdownWrap *wrap, const MarkdownSpan *span,
                     uint8_t variant) {
    size_t first = (*self).segmentCount;
    // A code span is one unbreakable unit; every other span wraps at spaces, so
    // `**two words**` does not become a single 11-cell word.
    bool placed = (*span).role == TERMINAL_MARKDOWN_CODE
                ? wrapWord(self, wrap, (*span).text, (*span).length, (*span).role, (*span).style)
                : appendPlain(self, wrap, (*span).text, (*span).length, (*span).role,
                              (*span).style);
    if (!placed)
        return false;
    return stampRange(self, first, variant, (*span).link, (*span).linkLength);
}

// Parse a source span COLD into flat spans, then place them. The parser owns the
// grammar (escapes, code runs, emphasis, links, images, autolinks); the view owns
// only word wrapping. A span buffer that overflowed still parsed the tail, so the
// bytes past the last emitted span are placed literally rather than dropped.
static bool emitSpans(TerminalMarkdownView *self, MarkdownWrap *wrap, const char *text,
                      size_t length, TerminalMarkdownRole base, uint16_t style,
                      uint8_t variant) {
    MarkdownSpan spans[MARKDOWN_INLINE_SPANS];
    bool complete = true;
    size_t count = TerminalMarkdownInline_parse(text, length, base, style, spans,
                                                MARKDOWN_INLINE_SPANS, &complete);
    for (size_t index = 0; index < count; ++index)
        if (!wrapSpan(self, wrap, &spans[index], variant))
            return false;
    if (complete || count == 0)
        return true;
    const MarkdownSpan *last = &spans[count - 1u];
    size_t consumed = (size_t) ((*last).text + (*last).length - text);
    if (consumed >= length)
        return true;
    return appendPlain(self, wrap, text + consumed, length - consumed, base, style);
}

static size_t leadingSpaces(const char *text, size_t length) {
    return TerminalMarkdownText_leadingSpaces(text, length);
}

static bool lineBlank(const char *text, size_t length) {
    return TerminalMarkdownText_blank(text, length);
}

static size_t trimRight(const char *text, size_t length) {
    return TerminalMarkdownText_trimRight(text, length);
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
    bool ordered = false;
    if (text[at] == '-' || text[at] == '*' || text[at] == '+') {
        ++at;
    } else if (text[at] >= '0' && text[at] <= '9') {
        while (at < length && text[at] >= '0' && text[at] <= '9')
            ++at;
        if (at == spaces || at >= length || (text[at] != '.' && text[at] != ')'))
            return marker;
        ++at;
        ordered = true;
    } else {
        return marker;
    }
    size_t markerEnd = at;
    if (at >= length || text[at] != ' ')
        return marker;
    ++at;
    if (at < length && text[at] == ' ')
        ++at;
    marker.valid = true;
    marker.ordered = ordered;
    marker.indent = spaces;
    marker.markerLength = markerEnd - spaces + 1u;
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
    // The level travels with every segment, so the theme can resolve a different
    // pen for `#` and `####` at render time without reflowing the document.
    if (!emitSpans(self, &wrap, text + bodyStart, length, TERMINAL_MARKDOWN_HEADING, 0,
                   (uint8_t) level))
        return false;
    if (level == 1u && (*self).width != 0)
        return emitRule(self, (*self).width);
    return true;
}

static bool emitParagraphLine(TerminalMarkdownView *self, MarkdownWrap *wrap, const char *text,
                              size_t length) {
    size_t spaces = leadingSpaces(text, length);
    if (spaces < length && spaces <= 3u)
        return emitSpans(self, wrap, text + spaces, length - spaces, TERMINAL_MARKDOWN_BODY, 0, 0);
    return emitSpans(self, wrap, text, length, TERMINAL_MARKDOWN_BODY, 0, 0);
}

// One fenced block, painted through whatever gutter the container uses. The
// prefix is the ONLY difference between a top-level fence, one inside a quote and
// one inside a callout: the line walk, the shading and the copy range are shared.
// `quoteDepth` is the number of `>` markers each source line carries (0 at top
// level), so the copy projection can strip exactly those.
static bool emitFenceBlock(TerminalMarkdownView *self, const char *const *lines, size_t lineCount,
                           size_t *index, const MarkdownPrefix *prefix,
                           char marker, size_t run, size_t quoteDepth) {
    size_t start = *index;
    size_t at = start + 1u;
    size_t contentEnd = at;
    size_t firstRow = (*self).rowCount;
    MarkdownWrap wrap = { .prefix = prefix, .width = (*self).width };
    bool first = true;
    while (at < lineCount) {
        const char *line = lines[at];
        size_t length = strlen(line);
        size_t contentOffset = 0;
        if (quoteDepth != 0) {
            size_t lineDepth = 0;
            size_t lineContent = 0;
            if (!quoteLine(line, length, &lineDepth, &lineContent) || lineDepth != quoteDepth)
                break;
            contentOffset = lineContent;
        }
        if (isClosingFence(line + contentOffset, length - contentOffset, marker, run)) {
            contentEnd = at;
            ++at;
            break;
        }
        contentEnd = at + 1u;
        if (!wrapStart(self, &wrap, first))
            return false;
        first = false;
        if (length > contentOffset
            && !wrapWord(self, &wrap, line + contentOffset, length - contentOffset,
                         TERMINAL_MARKDOWN_FENCE, 0))
            return false;
        ++at;
    }
    markCodeRows(self, firstRow);
    if (!TerminalMarkdownCodeBlockPool_add((*self).codeBlocks, firstRow,
                                           (*self).rowCount - firstRow, start + 1u,
                                           contentEnd - (start + 1u), quoteDepth))
        return false;
    *index = at;
    return true;
}

static bool emitFence(TerminalMarkdownView *self, const char *const *lines, size_t lineCount,
                      size_t *index, char marker, size_t run, size_t contentStart) {
    (void) contentStart;
    if (!emitGap(self))
        return false;
    MarkdownPrefix prefix = {0};
    prefixSpaces(&prefix, MARKDOWN_CODE_BLOCK_PAD, TERMINAL_MARKDOWN_FENCE, 0);
    prefix.repeatOnWrap = true;
    if (!emitFenceBlock(self, lines, lineCount, index, &prefix, marker, run, 0))
        return false;
    return emitGap(self);
}

static bool emitQuoteFence(TerminalMarkdownView *self, const char *const *lines,
                           size_t lineCount, size_t *index, char marker, size_t run,
                           size_t quoteDepth) {
    MarkdownPrefix prefix = {0};
    prefixQuote(&prefix, quoteDepth);
    prefix.repeatOnWrap = true;
    return emitFenceBlock(self, lines, lineCount, index, &prefix, marker, run, quoteDepth);
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
        size_t orderedMarkerWidth = marker.ordered ? marker.markerLength : 0u;
        if (marker.ordered) {
            size_t scan = at + 1u;
            while (scan < lineCount) {
                size_t scanLength = strlen(lines[scan]);
                if (lineBlank(lines[scan], scanLength)) {
                    ++scan;
                    continue;
                }
                MarkdownListMarker candidate = listMarker(lines[scan], scanLength);
                if (candidate.valid) {
                    if (candidate.ordered && candidate.indent == marker.indent
                        && candidate.markerLength > orderedMarkerWidth)
                        orderedMarkerWidth = candidate.markerLength;
                    if (candidate.indent <= marker.indent)
                        ++scan;
                    else
                        break;
                    continue;
                }
                if (leadingSpaces(lines[scan], scanLength) < marker.contentStart)
                    break;
                ++scan;
            }
        }
        MarkdownPrefix prefix = {0};
        size_t indent = marker.indent;
        if (indent > (*self).width / 2u)
            indent = (*self).width / 2u;
        prefixSpaces(&prefix, indent, TERMINAL_MARKDOWN_BODY, 0);
        prefixBeginMarker(&prefix);
        if (marker.task) {
            prefixAdd(&prefix, marker.taskDone ? "☑ " : "☐ ", 4u,
                      TERMINAL_MARKDOWN_MARKER, 0);
        } else if (marker.ordered) {
            if (orderedMarkerWidth > marker.markerLength)
                prefixSpaces(&prefix, orderedMarkerWidth - marker.markerLength,
                             TERMINAL_MARKDOWN_BODY, 0);
            prefixAdd(&prefix, lines[at] + marker.indent, marker.markerLength,
                      TERMINAL_MARKDOWN_MARKER, 0);
        } else {
            prefixAdd(&prefix, "• ", strlen("• "), TERMINAL_MARKDOWN_MARKER, 0);
        }
        prefixEndMarker(&prefix);
        MarkdownWrap wrap = { .prefix = &prefix, .width = (*self).width };
        size_t contentEnd = trimRight(lines[at], length);
        if (contentEnd > marker.contentStart) {
            if (!emitSpans(self, &wrap, lines[at] + marker.contentStart,
                           contentEnd - marker.contentStart, TERMINAL_MARKDOWN_BODY, 0, 0))
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
            if (!emitSpans(self, &wrap, lines[next] + spaces, end - spaces,
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

static bool startsBlock(const char *text, size_t length);

// The gutter every callout shares: the bar, one space, repeated on every wrapped
// row. Severity is the pen, not the shape, so a NOTE and a CAUTION read as the
// same kind of object with different urgency.
static void prefixCallout(MarkdownPrefix *prefix, TerminalMarkdownCalloutKind kind) {
    prefixAddVariant(prefix, TerminalMarkdownCallout_bar(),
                     strlen(TerminalMarkdownCallout_bar()),
                     TERMINAL_MARKDOWN_CALLOUT, TERMINAL_STYLE_BOLD, (uint8_t) kind);
    prefixAddVariant(prefix, " ", 1u, TERMINAL_MARKDOWN_CALLOUT, 0, (uint8_t) kind);
    (*prefix).repeatOnWrap = true;
}

// A callout: a bold `KIND` label on the bar's row, the optional title beside it,
// and an unbolded BODY-role body indented by the bar. The marker itself is never
// printed. Fences inside the aside keep the bar as their gutter.
static bool emitCallout(TerminalMarkdownView *self, const char *const *lines, size_t lineCount,
                        size_t *index, size_t depth, size_t contentStart,
                        TerminalMarkdownCallout callout) {
    if (!emitGap(self))
        return false;
    MarkdownPrefix prefix = {0};
    if (depth > 1u)
        prefixQuote(&prefix, depth - 1u);
    prefixCallout(&prefix, callout.kind);
    MarkdownWrap wrap = { .prefix = &prefix, .width = (*self).width };
    size_t at = *index;
    size_t length = strlen(lines[at]);
    size_t end = trimRight(lines[at], length);
    if (!wrapStart(self, &wrap, true))
        return false;
    const char *name = TerminalMarkdownCallout_name(callout.kind);
    size_t nameCells = cellsOf(name, strlen(name));
    size_t first = (*self).segmentCount;
    if (!appendSegment(self, name, strlen(name), 1u, TERMINAL_MARKDOWN_CALLOUT,
                       TERMINAL_STYLE_BOLD))
        return false;
    stampRange(self, first, (uint8_t) callout.kind, nullptr, 0);
    wrap.x += nameCells;
    wrap.content = true;
    if (contentStart < end) {
        wrapSpace(&wrap);
        if (!emitSpans(self, &wrap, lines[at] + contentStart, end - contentStart,
                       TERMINAL_MARKDOWN_CALLOUT, 0, (uint8_t) callout.kind))
            return false;
    }
    bool body = false;
    bool labelOpen = true;
    ++at;
    while (at < lineCount) {
        size_t lineLength = strlen(lines[at]);
        size_t content = 0;
        if (depth != 0) {
            size_t lineDepth = 0;
            if (!quoteLine(lines[at], lineLength, &lineDepth, &content) || lineDepth != depth)
                break;
        }
        char fenceMarker = 0;
        size_t fenceRun = 0;
        size_t fenceOffset = 0;
        bool fence = isFence(lines[at] + content, lineLength - content,
                             &fenceMarker, &fenceRun, &fenceOffset);
        if (!fence && depth == 0 && startsBlock(lines[at], lineLength))
            break;
        size_t lineEnd = trimRight(lines[at], lineLength);
        if (fence) {
            if (labelOpen && !body) {
                if (!wrapBreak(self, &wrap))
                    return false;
                labelOpen = false;
            }
            wrap.open = false;
            wrap.started = false;
            wrap.content = false;
            if (!emitGap(self))
                return false;
            size_t blockStart = at;
            if (!emitFenceBlock(self, lines, lineCount, &at, &prefix,
                                fenceMarker, fenceRun, depth))
                return false;
            if (at == blockStart)
                ++at;
            if (!emitGap(self))
                return false;
            body = true;
            continue;
        }
        if (lineEnd <= content) {
            if (depth == 0)
                break;
            if (wrap.open && wrap.content && !wrapBreak(self, &wrap))
                return false;
            ++at;
            continue;
        }
        if (!body) {
            if (labelOpen && !wrapBreak(self, &wrap))
                return false;
            labelOpen = false;
            body = true;
        } else {
            wrapSpace(&wrap);
        }
        if (!emitSpans(self, &wrap, lines[at] + content, lineEnd - content,
                       TERMINAL_MARKDOWN_BODY, 0, 0))
            return false;
        ++at;
    }
    *index = at;
    return true;
}

// A blockquote, or a callout when its first content span names a severity. The
// two spellings of a callout — GFM's `> [!NOTE]` and a README's `!NOTE!` — both
// land here; only the marker is different, so only the marker is parsed away.
static bool emitQuote(TerminalMarkdownView *self, const char *const *lines, size_t lineCount,
                      size_t *index) {
    if (!emitGap(self))
        return false;
    size_t at = *index;
    size_t depth = 0;
    size_t contentStart = 0;
    if (!quoteLine(lines[at], strlen(lines[at]), &depth, &contentStart))
        return false;
    size_t length = strlen(lines[at]);
    size_t end = trimRight(lines[at], length);
    TerminalMarkdownCallout callout =
        TerminalMarkdownCallout_parse(lines[at] + contentStart, end > contentStart
                                                             ? end - contentStart : 0u);
    if (callout.valid)
        return emitCallout(self, lines, lineCount, index, depth,
                           contentStart + callout.contentStart, callout);
    MarkdownPrefix prefix = {0};
    prefixQuote(&prefix, depth);
    MarkdownWrap wrap = { .prefix = &prefix, .width = (*self).width };
    while (at < lineCount) {
        size_t lineLength = strlen(lines[at]);
        size_t lineDepth = 0;
        size_t lineContent = 0;
        if (!quoteLine(lines[at], lineLength, &lineDepth, &lineContent))
            break;
        if (lineDepth != depth)
            break;
        char fenceMarker = 0;
        size_t fenceRun = 0;
        size_t fenceOffset = 0;
        if (isFence(lines[at] + lineContent, lineLength - lineContent,
                    &fenceMarker, &fenceRun, &fenceOffset)) {
            if (!emitGap(self))
                return false;
            wrap.open = false;
            wrap.started = false;
            wrap.pendingSpace = false;
            wrap.content = false;
            if (!emitQuoteFence(self, lines, lineCount, &at, fenceMarker, fenceRun, depth))
                return false;
            if (!emitGap(self))
                return false;
            continue;
        }
        size_t lineEnd = trimRight(lines[at], lineLength);
        if (lineEnd > lineContent) {
            wrapSpace(&wrap);
            if (!emitSpans(self, &wrap, lines[at] + lineContent, lineEnd - lineContent,
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

static bool tableCandidate(const char *text, size_t length, const char *next, size_t nextLength,
                           size_t *columns) {
    const char *cells[TERMINAL_MARKDOWN_TABLE_COLUMNS];
    size_t lengths[TERMINAL_MARKDOWN_TABLE_COLUMNS];
    if (TerminalMarkdownTable_split(text, length, cells, lengths,
                                    TERMINAL_MARKDOWN_TABLE_COLUMNS) < 2u)
        return false;
    size_t count = TerminalMarkdownTable_parseDelimiter(next, nextLength, nullptr, 0);
    if (count < 2u)
        return false;
    if (columns != nullptr)
        *columns = count;
    return true;
}

static bool appendTableSpaces(TerminalMarkdownView *self, size_t cells) {
    while (cells != 0u) {
        size_t chunk = cells > MARKDOWN_SPACES_CAP ? MARKDOWN_SPACES_CAP : cells;
        if (!appendSegment(self, MARKDOWN_SPACES, chunk, 1u, TERMINAL_MARKDOWN_BODY, 0))
            return false;
        cells -= chunk;
    }
    return true;
}

// Place an already-wrapped cell slice exactly where the width policy put it: no
// re-wrapping, because the table decided the break. Inline styling still applies,
// so `**bold**` and `` `code` `` in a cell lose their markers like they do in a
// paragraph.
static bool appendCellSpans(TerminalMarkdownView *self, const char *text, size_t length,
                            TerminalMarkdownRole base, uint16_t style) {
    MarkdownSpan spans[MARKDOWN_INLINE_SPANS];
    bool complete = true;
    size_t count = TerminalMarkdownInline_parse(text, length, base, style, spans,
                                                MARKDOWN_INLINE_SPANS, &complete);
    size_t consumed = 0;
    for (size_t index = 0; index < count; ++index) {
        const MarkdownSpan *span = &spans[index];
        size_t first = (*self).segmentCount;
        if (!appendSegment(self, (*span).text, (*span).length, 1u, (*span).role, (*span).style))
            return false;
        stampRange(self, first, 0, (*span).link, (*span).linkLength);
        consumed = (size_t) ((*span).text + (*span).length - text);
    }
    if (complete || consumed >= length)
        return true;
    return appendSegment(self, text + consumed, length - consumed, 1u, base, style);
}

// One visual line of one cell: alignment padding, the styled text, and — only
// when the width policy had to cut inside a word — an honest ellipsis.
static bool emitCellLine(TerminalMarkdownView *self, const TerminalMarkdownTableCellLine *line,
                         size_t width, TerminalMarkdownTableAlign align,
                         TerminalMarkdownRole role, uint16_t style) {
    size_t used = (*line).cells;
    if (used > width)
        used = width;
    size_t slack = width - used;
    size_t lead = align == TERMINAL_MARKDOWN_TABLE_CENTER ? slack / 2u
                : align == TERMINAL_MARKDOWN_TABLE_RIGHT ? slack : 0u;
    if (lead != 0 && !appendTableSpaces(self, lead))
        return false;
    if (!appendCellSpans(self, (*line).text, (*line).length, role, style))
        return false;
    size_t tail = slack - lead;
    if ((*line).truncated) {
        if (!appendSegment(self, "…", strlen("…"), 1u, role, style))
            return false;
        if (tail != 0)
            --tail;
    }
    return tail == 0 || appendTableSpaces(self, tail);
}

static bool emitTableBorder(TerminalMarkdownView *self, const size_t *widths,
                            size_t columns, size_t kind) {
    const char *left = kind == 0u ? MARKDOWN_BORDER_TOP
                      : kind == 1u ? MARKDOWN_BORDER_MIDDLE : MARKDOWN_BORDER_BOTTOM;
    const char *join = kind == 0u ? MARKDOWN_BORDER_TOP_JOIN
                      : kind == 1u ? MARKDOWN_BORDER_MIDDLE_JOIN : MARKDOWN_BORDER_BOTTOM_JOIN;
    const char *right = kind == 0u ? MARKDOWN_BORDER_RIGHT
                       : kind == 1u ? MARKDOWN_BORDER_RIGHT_MIDDLE
                                    : MARKDOWN_BORDER_RIGHT_BOTTOM;
    if (!appendRow(self)
        || !appendSegment(self, left, strlen(left), 1u, TERMINAL_MARKDOWN_TABLE_BORDER, 0))
        return false;
    for (size_t column = 0; column < columns; ++column) {
        if (!appendSegment(self, MARKDOWN_BORDER_HORIZONTAL,
                           strlen(MARKDOWN_BORDER_HORIZONTAL), widths[column] + 2u,
                           TERMINAL_MARKDOWN_TABLE_BORDER, 0))
            return false;
        if (column + 1u < columns
            && !appendSegment(self, join, strlen(join), 1u,
                              TERMINAL_MARKDOWN_TABLE_BORDER, 0))
            return false;
    }
    return appendSegment(self, right, strlen(right), 1u, TERMINAL_MARKDOWN_TABLE_BORDER, 0);
}

static bool emitTableRow(TerminalMarkdownView *self, const char *text, size_t length,
                         size_t columns, const size_t *widths,
                         const TerminalMarkdownTableAlign *aligns, bool heading) {
    const char *cells[TERMINAL_MARKDOWN_TABLE_COLUMNS];
    size_t lengths[TERMINAL_MARKDOWN_TABLE_COLUMNS];
    size_t count = TerminalMarkdownTable_split(text, length, cells, lengths,
                                               TERMINAL_MARKDOWN_TABLE_COLUMNS);
    size_t offsets[TERMINAL_MARKDOWN_TABLE_COLUMNS] = {0};
    size_t heights[TERMINAL_MARKDOWN_TABLE_COLUMNS];
    size_t rowHeight = 1u;
    for (size_t column = 0; column < columns; ++column) {
        heights[column] = column < count
                        ? TerminalMarkdownTable_lineCount(cells[column], lengths[column],
                                                          widths[column])
                        : 1u;
        if (heights[column] > rowHeight)
            rowHeight = heights[column];
    }
    TerminalMarkdownRole role = heading ? TERMINAL_MARKDOWN_TABLE_HEADER
                                        : TERMINAL_MARKDOWN_BODY;
    for (size_t visualRow = 0; visualRow < rowHeight; ++visualRow) {
        if (!appendRow(self)
            || !appendSegment(self, MARKDOWN_BORDER_VERTICAL,
                              strlen(MARKDOWN_BORDER_VERTICAL), 1u,
                              TERMINAL_MARKDOWN_TABLE_BORDER, 0)
            || !appendSegment(self, " ", 1u, 1u, TERMINAL_MARKDOWN_BODY, 0))
            return false;
        for (size_t column = 0; column < columns; ++column) {
            TerminalMarkdownTableCellLine line = {0};
            if (visualRow < heights[column]) {
                const char *cell = column < count ? cells[column] : "";
                size_t cellLength = column < count ? lengths[column] : 0u;
                line = TerminalMarkdownTable_cellLine(cell, cellLength, offsets[column],
                                                      widths[column]);
                offsets[column] = line.next;
            }
            if (!emitCellLine(self, &line, widths[column], aligns[column], role, 0))
                return false;
            if (column + 1u < columns) {
                if (!appendSegment(self, " ", 1u, 1u, TERMINAL_MARKDOWN_BODY, 0)
                    || !appendSegment(self, MARKDOWN_BORDER_VERTICAL,
                                      strlen(MARKDOWN_BORDER_VERTICAL), 1u,
                                      TERMINAL_MARKDOWN_TABLE_BORDER, 0)
                    || !appendSegment(self, " ", 1u, 1u, TERMINAL_MARKDOWN_BODY, 0))
                    return false;
            }
        }
        if (!appendSegment(self, " ", 1u, 1u, TERMINAL_MARKDOWN_BODY, 0)
            || !appendSegment(self, MARKDOWN_BORDER_VERTICAL,
                              strlen(MARKDOWN_BORDER_VERTICAL), 1u,
                              TERMINAL_MARKDOWN_TABLE_BORDER, 0))
            return false;
    }
    return true;
}

// The honest fallback when even the longest words cannot share the viewport: a
// stacked label/value list instead of a clipped box. Nothing is cut; the table
// simply stops pretending to be a grid.
static bool emitTableCompact(TerminalMarkdownView *self, const char *const *lines,
                             size_t header, size_t end, size_t columns) {
    const char *headers[TERMINAL_MARKDOWN_TABLE_COLUMNS];
    size_t headerLengths[TERMINAL_MARKDOWN_TABLE_COLUMNS];
    size_t headerCount = TerminalMarkdownTable_split(lines[header], strlen(lines[header]),
                                                     headers, headerLengths,
                                                     TERMINAL_MARKDOWN_TABLE_COLUMNS);
    if (!emitGap(self))
        return false;
    for (size_t row = header + 2u; row < end; ++row) {
        const char *cells[TERMINAL_MARKDOWN_TABLE_COLUMNS];
        size_t lengths[TERMINAL_MARKDOWN_TABLE_COLUMNS];
        size_t count = TerminalMarkdownTable_split(lines[row], strlen(lines[row]), cells, lengths,
                                                   TERMINAL_MARKDOWN_TABLE_COLUMNS);
        for (size_t column = 0; column < columns; ++column) {
            MarkdownPrefix prefix = {0};
            MarkdownWrap wrap = { .prefix = &prefix, .width = (*self).width };
            if (column < headerCount && headerLengths[column] != 0u) {
                if (!emitSpans(self, &wrap, headers[column], headerLengths[column],
                               TERMINAL_MARKDOWN_TABLE_HEADER, 0, 0))
                    return false;
                wrapSpace(&wrap);
                if (!appendPlain(self, &wrap, ":", 1u, TERMINAL_MARKDOWN_BODY, 0))
                    return false;
                wrapSpace(&wrap);
            }
            if (column < count && lengths[column] != 0u) {
                if (!emitSpans(self, &wrap, cells[column], lengths[column],
                               TERMINAL_MARKDOWN_BODY, 0, 0))
                    return false;
            } else if (!wrapStart(self, &wrap, true)) {
                return false;
            }
        }
    }
    return true;
}

static bool emitTable(TerminalMarkdownView *self, const char *const *lines, size_t lineCount,
                      size_t *index, size_t columns) {
    if (columns > TERMINAL_MARKDOWN_TABLE_COLUMNS)
        columns = TERMINAL_MARKDOWN_TABLE_COLUMNS;
    const char *cells[TERMINAL_MARKDOWN_TABLE_COLUMNS];
    size_t lengths[TERMINAL_MARKDOWN_TABLE_COLUMNS];
    size_t header = *index;
    TerminalMarkdownTableAlign aligns[TERMINAL_MARKDOWN_TABLE_COLUMNS] = {0};
    size_t declared = TerminalMarkdownTable_parseDelimiter(lines[header + 1u],
                                                           strlen(lines[header + 1u]),
                                                           aligns,
                                                           TERMINAL_MARKDOWN_TABLE_COLUMNS);
    if (declared < columns)
        columns = declared;
    size_t body = header + 2u;
    size_t end = body;
    while (end < lineCount) {
        size_t length = strlen(lines[end]);
        if (lineBlank(lines[end], length)
            || findBytes(lines[end], length, 0, "|", 1u) == MARKDOWN_NO_INDEX)
            break;
        ++end;
    }
    size_t needed[TERMINAL_MARKDOWN_TABLE_COLUMNS] = {0};
    size_t minimums[TERMINAL_MARKDOWN_TABLE_COLUMNS] = {0};
    size_t widths[TERMINAL_MARKDOWN_TABLE_COLUMNS] = {0};
    for (size_t row = header; row < end; ++row) {
        size_t count = TerminalMarkdownTable_split(lines[row], strlen(lines[row]), cells, lengths,
                                                   TERMINAL_MARKDOWN_TABLE_COLUMNS);
        for (size_t column = 0; column < columns; ++column) {
            if (column >= count)
                continue;
            size_t natural = TerminalMarkdownTable_natural(cells[column], lengths[column]);
            if (natural > needed[column])
                needed[column] = natural;
            size_t word = TerminalMarkdownTable_longestWord(cells[column], lengths[column]);
            if (word > minimums[column])
                minimums[column] = word;
        }
    }
    for (size_t column = 0; column < columns; ++column) {
        if (needed[column] == 0u)
            needed[column] = 1u;
        if (minimums[column] == 0u)
            minimums[column] = 1u;
    }
    if (!TerminalMarkdownTable_allocate(needed, minimums, widths, columns, (*self).width)) {
        if (!emitTableCompact(self, lines, header, end, columns))
            return false;
        *index = end;
        return true;
    }
    if (!emitGap(self))
        return false;
    if (!emitTableBorder(self, widths, columns, 0u)
        || !emitTableRow(self, lines[header], strlen(lines[header]), columns, widths,
                         aligns, true))
        return false;
    if (body == end) {
        if (!emitTableBorder(self, widths, columns, 2u))
            return false;
        *index = end;
        return true;
    }
    if (!emitTableBorder(self, widths, columns, 1u))
        return false;
    for (size_t row = body; row < end; ++row) {
        if (!emitTableRow(self, lines[row], strlen(lines[row]), columns, widths, aligns, false))
            return false;
        if (!emitTableBorder(self, widths, columns, row + 1u < end ? 1u : 2u))
            return false;
    }
    *index = end;
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
    TerminalMarkdownCodeBlockPool_reset((*self).codeBlocks);
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
        size_t calloutSpaces = leadingSpaces(line, length);
        if (calloutSpaces <= 3u && calloutSpaces < length && line[calloutSpaces] == '!') {
            size_t calloutEnd = trimRight(line, length);
            TerminalMarkdownCallout callout =
                TerminalMarkdownCallout_parse(line + calloutSpaces, calloutEnd - calloutSpaces);
            if (callout.valid) {
                if (!emitCallout(self, (*self).lines, (*self).lineCount, &index, 0,
                                 calloutSpaces + callout.contentStart, callout))
                    break;
                continue;
            }
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
    (*self).codeBlocks = TerminalMarkdownCodeBlockPool_0();
    if ((*self).codeBlocks == nullptr) {
        Memory_free(self);
        return nullptr;
    }
    (*self).theme = TerminalMarkdownTheme_0();
    (*self).style = TerminalMarkdownTheme_style(&(*self).theme);
    (*self).hoverRow = MARKDOWN_NO_INDEX;
    (*self).hoverColumn = MARKDOWN_NO_INDEX;
    (*self).copiedBlock = MARKDOWN_NO_INDEX;
    return self;
}

void TerminalMarkdownView_free(TerminalMarkdownView *self) {
    if (self == nullptr)
        return;
    Memory_free((*self).rows);
    Memory_free((*self).segments);
    TerminalMarkdownCodeBlockPool_free((*self).codeBlocks);
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
    TerminalMarkdownCodeBlockPool_reset((*self).codeBlocks);
    (*self).hoverRow = MARKDOWN_NO_INDEX;
    (*self).hoverColumn = MARKDOWN_NO_INDEX;
    (*self).copiedBlock = MARKDOWN_NO_INDEX;
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
    TerminalMarkdownCodeBlockPool_reset((*self).codeBlocks);
    (*self).copiedBlock = MARKDOWN_NO_INDEX;
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

size_t TerminalMarkdownView_codeBlockCount(const TerminalMarkdownView *self) {
    return self != nullptr ? TerminalMarkdownCodeBlockPool_count((*self).codeBlocks) : 0u;
}

TerminalMarkdownCodeBlock TerminalMarkdownView_codeBlockAt(const TerminalMarkdownView *self,
                                                           size_t row) {
    TerminalMarkdownCodeBlock none = {0};
    if (self == nullptr)
        return none;
    size_t index = 0;
    if (!TerminalMarkdownCodeBlockPool_indexAt((*self).codeBlocks, row, &index))
        return none;
    return TerminalMarkdownCodeBlockPool_at((*self).codeBlocks, index);
}

TerminalMarkdownCodeLine TerminalMarkdownView_codeBlockLine(const TerminalMarkdownView *self,
                                                            TerminalMarkdownCodeBlock block,
                                                            size_t line) {
    if (self == nullptr)
        return (TerminalMarkdownCodeLine) {0};
    return TerminalMarkdownCodeBlock_lineOf((*self).lines, (*self).lineCount, block, line);
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
    (*self).theme = TerminalMarkdownTheme_fromStyle(style);
    return true;
}

TerminalMarkdownStyle TerminalMarkdownView_getStyle(const TerminalMarkdownView *self) {
    return self != nullptr ? (*self).style : (TerminalMarkdownStyle) {0};
}

bool TerminalMarkdownView_setTheme(TerminalMarkdownView *self, TerminalMarkdownTheme theme) {
    if (self == nullptr) {
        THROW("TerminalMarkdownView theme: null view");
        return false;
    }
    (*self).theme = theme;
    (*self).style = TerminalMarkdownTheme_style(&(*self).theme);
    return true;
}

TerminalMarkdownTheme TerminalMarkdownView_getTheme(const TerminalMarkdownView *self) {
    return self != nullptr ? (*self).theme : (TerminalMarkdownTheme) {0};
}

// Draw the copy affordance of one fence on its first row. It is painted AFTER
// the text so it owns its cells even when a code line runs to the right edge; the
// glyph is the theme's, and `copiedBlock` decides which of the two it is.
static void renderCopyIcon(const TerminalMarkdownView *self, TerminalCanvas *canvas,
                           size_t screenRow, size_t width, size_t blockIndex,
                           size_t glyphCells) {
    const TerminalMarkdownTheme *theme = &(*self).theme;
    bool copied = (*self).copiedBlock == blockIndex;
    const char *glyph = copied ? (*theme).copiedGlyph : (*theme).copyGlyph;
    if (glyph == nullptr)
        return;
    if (width < glyphCells + 1u)
        return;
    size_t column = TerminalMarkdownAction_iconColumn(width, glyphCells);
    TerminalPen pen = copied ? (*theme).copied : (*theme).copy;
    TerminalCanvas_text(canvas, column, screenRow, glyph, strlen(glyph), pen);
}

void TerminalMarkdownView_render(const TerminalMarkdownView *self, TerminalCanvas *canvas) {
    if (self == nullptr || canvas == nullptr || !TerminalCanvas_isValid(canvas))
        return;
    if (!(*self).ready)
        return;
    const TerminalMarkdownTheme *theme = &(*self).theme;
    const char *copyGlyph = (*theme).copyGlyph;
    size_t visible = TerminalCanvas_getHeight(canvas);
    size_t width = TerminalCanvas_getWidth(canvas);
    size_t glyphCells = cellsOf(copyGlyph, copyGlyph != nullptr ? strlen(copyGlyph) : 0u);
    if (glyphCells == 0)
        glyphCells = 1u;
    for (size_t row = 0; row < visible && (*self).scroll + row < (*self).rowCount; ++row) {
        size_t documentRow = (*self).scroll + row;
        const MarkdownRow *record = &(*self).rows[documentRow];
        size_t blockIndex = MARKDOWN_NO_INDEX;
        bool firstBlockRow = false;
        if ((*record).codeBlock
            && TerminalMarkdownCodeBlockPool_indexAt((*self).codeBlocks, documentRow,
                                                     &blockIndex)) {
            // The wash is painted UNDER the text so code glyph cells keep their own
            // foreground while every cell of the row carries the block background.
            TerminalPen block = TerminalMarkdownTheme_pen(&(*self).theme,
                                                          TERMINAL_MARKDOWN_FENCE, 0, false);
            block.style = 0;
            block.fg = TERMINAL_COLOR_DEFAULT;
            TerminalCanvas_hline(canvas, row, ' ', block);
            TerminalMarkdownCodeBlock recordBlock =
                TerminalMarkdownCodeBlockPool_at((*self).codeBlocks, blockIndex);
            firstBlockRow = recordBlock.firstRow == documentRow;
        }
        size_t x = 0;
        for (size_t index = 0; index < (*record).segmentCount; ++index) {
            const MarkdownSegment *segment = &(*self).segments[(*record).firstSegment + index];
            size_t advance = cellsOf((*segment).text, (*segment).length);
            bool hovered = (*segment).link != nullptr
                        && (*self).hoverRow == documentRow
                        && (*self).hoverColumn >= x
                        && ((*self).hoverColumn < x + advance
                            || ((*self).hoverColumn == x && advance == 0));
            TerminalPen pen = TerminalMarkdownTheme_pen(&(*self).theme, (*segment).role,
                                                        (*segment).variant, hovered);
            pen.style = (uint16_t) (pen.style | (*segment).style);
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
        if (firstBlockRow && blockIndex != MARKDOWN_NO_INDEX)
            renderCopyIcon(self, canvas, row, width, blockIndex, glyphCells);
    }
}

// What a press on this DOCUMENT cell means. The view reports it; it never opens
// the link and never touches the clipboard — that is the caller's tier.
TerminalMarkdownAction TerminalMarkdownView_actionAt(const TerminalMarkdownView *self,
                                                     size_t row, size_t column) {
    TerminalMarkdownAction none = {0};
    if (self == nullptr || !(*self).ready || row >= (*self).rowCount)
        return none;
    size_t blockIndex = MARKDOWN_NO_INDEX;
    if (TerminalMarkdownCodeBlockPool_indexAt((*self).codeBlocks, row, &blockIndex)) {
        TerminalMarkdownCodeBlock block =
            TerminalMarkdownCodeBlockPool_at((*self).codeBlocks, blockIndex);
        if (block.firstRow == row) {
            const TerminalMarkdownTheme *theme = &(*self).theme;
            const char *copyGlyph = (*theme).copyGlyph;
            size_t glyphCells = cellsOf(copyGlyph,
                                        copyGlyph != nullptr ? strlen(copyGlyph) : 0u);
            if (glyphCells == 0)
                glyphCells = 1u;
            if (TerminalMarkdownAction_iconHit((*self).width, column, glyphCells))
                return (TerminalMarkdownAction) { TERMINAL_MARKDOWN_ACTION_COPY_CODE,
                                                  nullptr, 0, blockIndex };
        }
    }
    const MarkdownRow *record = &(*self).rows[row];
    size_t x = 0;
    for (size_t index = 0; index < (*record).segmentCount; ++index) {
        const MarkdownSegment *segment = &(*self).segments[(*record).firstSegment + index];
        size_t advance = cellsOf((*segment).text, (*segment).length) * (*segment).repeat;
        if ((*segment).link != nullptr && column >= x && column < x + advance)
            return (TerminalMarkdownAction) { TERMINAL_MARKDOWN_ACTION_LINK, (*segment).link,
                                              (*segment).linkLength, 0 };
        if (advance > SIZE_MAX - x)
            break;
        x += advance;
    }
    return none;
}

bool TerminalMarkdownView_setHover(TerminalMarkdownView *self, size_t row, size_t column) {
    if (self == nullptr)
        return false;
    (*self).hoverRow = row;
    (*self).hoverColumn = column;
    return TerminalMarkdownView_actionAt(self, row, column).kind
           != TERMINAL_MARKDOWN_ACTION_NONE;
}

void TerminalMarkdownView_clearHover(TerminalMarkdownView *self) {
    if (self == nullptr)
        return;
    (*self).hoverRow = MARKDOWN_NO_INDEX;
    (*self).hoverColumn = MARKDOWN_NO_INDEX;
}

bool TerminalMarkdownView_copyBlock(const TerminalMarkdownView *self, size_t block,
                                    char *dest, size_t capacity, size_t *written) {
    if (self == nullptr)
        return false;
    return TerminalMarkdownCodeBlockPool_copy((*self).codeBlocks, (*self).lines,
                                              (*self).lineCount, block, dest, capacity, written);
}

bool TerminalMarkdownView_setCopied(TerminalMarkdownView *self, size_t block, bool copied) {
    if (self == nullptr)
        return false;
    if (!copied) {
        if (block == MARKDOWN_NO_INDEX || (*self).copiedBlock == block)
            (*self).copiedBlock = MARKDOWN_NO_INDEX;
        return true;
    }
    if (block >= TerminalMarkdownCodeBlockPool_count((*self).codeBlocks))
        return false;
    (*self).copiedBlock = block;
    return true;
}

bool TerminalMarkdownView_isCopied(const TerminalMarkdownView *self, size_t block) {
    if (self == nullptr || block == MARKDOWN_NO_INDEX)
        return false;
    return (*self).copiedBlock == block
        && block < TerminalMarkdownCodeBlockPool_count((*self).codeBlocks);
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
        "segmentCapacity=%zu,codeBlocks=<owned:%zu>,theme=<pens>,hoverRow=%zu,"
        "hoverColumn=%zu,copiedBlock=%zu,refusals=%zu}",
        (*self).lineCount, (*self).lineCount, (*self).dirty, (*self).ready,
        (*self).truncated, (*self).failed, (*self).width, (*self).visibleRows,
        (*self).scroll, (*self).rowCapacity, (*self).rowCount, (*self).rowCapacity,
        (*self).segmentCapacity, (*self).segmentCount, (*self).segmentCapacity,
        TerminalMarkdownCodeBlockPool_count((*self).codeBlocks),
        (*self).hoverRow, (*self).hoverColumn, (*self).copiedBlock, (*self).refusals);
}
