#include "ui/markdown/terminal_markdown_view.h"
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
    bool codeBlock;   // this row is part of a shaded fenced code block
} MarkdownRow;

// PRIVATE HELPER: MarkdownCodeBlockRecord (no behaviour).
// Laid-out fence bookkeeping. `valid` is implicit (only appended records exist).
typedef struct MarkdownCodeBlockRecord {
    size_t firstRow;
    size_t rowCount;
    size_t firstSourceLine;
    size_t sourceLineCount;
    size_t quoteDepth;
} MarkdownCodeBlockRecord;

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

typedef struct MarkdownCellLine {
    const char *text;
    size_t length;
    size_t next;
    size_t cells;
} MarkdownCellLine;

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
    MarkdownCodeBlockRecord *codeBlocks;
    size_t codeBlockCount;
    size_t codeBlockCapacity;
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

static bool growCodeBlocks(TerminalMarkdownView *self) {
    size_t next = (*self).codeBlockCapacity == 0 ? TERMINAL_MARKDOWN_SEGMENTS_DEFAULT
                                                 : (*self).codeBlockCapacity * 2u;
    if (next < (*self).codeBlockCapacity
        || next > SIZE_MAX / sizeof(MarkdownCodeBlockRecord)) {
        refuse(self, "code block capacity overflow");
        return false;
    }
    MarkdownCodeBlockRecord *grown =
        Memory_alloc(TYPE_BYTE_ARRAY, next * sizeof(MarkdownCodeBlockRecord));
    if (grown == nullptr) {
        refuse(self, "code block pool allocation refused");
        return false;
    }
    if ((*self).codeBlockCapacity != 0)
        memcpy(grown, (*self).codeBlocks,
               (*self).codeBlockCapacity * sizeof(MarkdownCodeBlockRecord));
    Memory_free((*self).codeBlocks);
    (*self).codeBlocks = grown;
    (*self).codeBlockCapacity = next;
    return true;
}

static bool appendCodeBlock(TerminalMarkdownView *self, size_t firstRow, size_t rowCount,
                            size_t firstSourceLine, size_t sourceLineCount, size_t quoteDepth) {
    if (rowCount == 0)
        return true;
    if ((*self).codeBlockCount == (*self).codeBlockCapacity && !growCodeBlocks(self))
        return false;
    (*self).codeBlocks[(*self).codeBlockCount] =
        (MarkdownCodeBlockRecord) { firstRow, rowCount, firstSourceLine, sourceLineCount,
                                    quoteDepth };
    ++(*self).codeBlockCount;
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
    if (prefix == nullptr || (*prefix).count >= MARKDOWN_PREFIX_ITEMS || text == nullptr)
        return;
    MarkdownPrefixItem *item = &(*prefix).items[(*prefix).count];
    (*item).text = text;
    (*item).length = length;
    (*item).cells = cellsOf(text, length);
    (*item).role = role;
    (*item).style = style;
    (*prefix).cells += (*item).cells;
    ++(*prefix).count;
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
            if (!appendSegment(self, (*item).text, (*item).length, 1u, (*item).role, (*item).style))
                return false;
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
                     size_t length, TerminalMarkdownRole role, uint8_t style) {
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
        if (!appendSegment(self, " ", 1u, 1u, TERMINAL_MARKDOWN_BODY, 0))
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

static bool tableDelimiter(const char *text, size_t length, size_t *columns) {
    size_t count = 0;
    size_t at = 0;
    if (length == 0)
        return false;
    if (text[at] == '|')
        ++at;
    while (at < length) {
        size_t start = at;
        while (at < length && text[at] != '|')
            ++at;
        size_t cellLength = at - start;
        while (cellLength > 0 && isSpace(text[start]))
            ++start, --cellLength;
        while (cellLength > 0 && isSpace(text[start + cellLength - 1u]))
            --cellLength;
        if (cellLength == 0 && count != 0 && at + 1u == length && text[at] == '|')
            break;
        if (cellLength == 0)
            return false;
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
        ++count;
        if (at == length)
            break;
        ++at;
        if (at == length)
            break;
    }
    *columns = count;
    return count >= 2u;
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

// Extract one DISPLAY/COPY line of a fenced block. A top-level fence uses the
// source line verbatim (its indentation is code); a fence inside a quote strips the
// quote markers first. False means the line does not carry the expected quote depth.
static bool fenceContentText(const char *line, size_t quoteDepth, const char **text,
                             size_t *length) {
    size_t lineLength = strlen(line);
    size_t start = 0;
    if (quoteDepth != 0) {
        size_t depth = 0;
        size_t contentStart = 0;
        if (!quoteLine(line, lineLength, &depth, &contentStart) || depth != quoteDepth)
            return false;
        start = contentStart;
    }
    if (start > lineLength)
        start = lineLength;
    *text = line + start;
    *length = lineLength - start;
    return true;
}

static bool emitFence(TerminalMarkdownView *self, const char *const *lines, size_t lineCount,
                      size_t *index, char marker, size_t run, size_t contentStart) {
    (void) contentStart;
    if (!emitGap(self))
        return false;
    size_t start = *index;
    size_t at = start + 1u;
    size_t contentEnd = at;
    size_t firstRow = (*self).rowCount;
    MarkdownPrefix prefix = {0};
    prefixSpaces(&prefix, MARKDOWN_CODE_BLOCK_PAD, TERMINAL_MARKDOWN_FENCE, 0);
    prefix.repeatOnWrap = true;
    MarkdownWrap wrap = { .prefix = &prefix, .width = (*self).width };
    bool first = true;
    while (at < lineCount) {
        size_t length = strlen(lines[at]);
        if (isClosingFence(lines[at], length, marker, run)) {
            contentEnd = at;
            ++at;
            break;
        }
        contentEnd = at + 1u;
        if (!wrapStart(self, &wrap, first))
            return false;
        first = false;
        if (length != 0
            && !wrapWord(self, &wrap, lines[at], length, TERMINAL_MARKDOWN_FENCE, 0))
            return false;
        ++at;
    }
    markCodeRows(self, firstRow);
    if (!appendCodeBlock(self, firstRow, (*self).rowCount - firstRow, start + 1u,
                         contentEnd - (start + 1u), 0))
        return false;
    if (!emitGap(self))
        return false;
    *index = at;
    return true;
}

// A fence INSIDE a blockquote/callout: the quote gutter repeats on every shaded
// row, the code content has its quote markers stripped, and the copy projection
// remembers the depth so it can strip them too.
static bool emitQuoteFence(TerminalMarkdownView *self, const char *const *lines,
                           size_t lineCount, size_t *index, char marker, size_t run,
                           size_t quoteDepth) {
    size_t start = *index;
    size_t at = start + 1u;
    size_t contentEnd = at;
    size_t firstRow = (*self).rowCount;
    MarkdownPrefix prefix = {0};
    prefixQuote(&prefix, quoteDepth);
    prefix.repeatOnWrap = true;
    MarkdownWrap wrap = { .prefix = &prefix, .width = (*self).width };
    bool first = true;
    while (at < lineCount) {
        const char *line = lines[at];
        size_t length = strlen(line);
        size_t depth = 0;
        size_t lineContent = 0;
        if (!quoteLine(line, length, &depth, &lineContent) || depth != quoteDepth)
            break;
        if (isClosingFence(line + lineContent, length - lineContent, marker, run)) {
            contentEnd = at;
            ++at;
            break;
        }
        contentEnd = at + 1u;
        size_t textLength = length - lineContent;
        if (!wrapStart(self, &wrap, first))
            return false;
        first = false;
        if (textLength != 0
            && !wrapWord(self, &wrap, line + lineContent, textLength,
                         TERMINAL_MARKDOWN_FENCE, 0))
            return false;
        ++at;
    }
    markCodeRows(self, firstRow);
    if (!appendCodeBlock(self, firstRow, (*self).rowCount - firstRow, start + 1u,
                         contentEnd - (start + 1u), quoteDepth))
        return false;
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
    bool callout = false;
    while (at < lineCount) {
        size_t length = strlen(lines[at]);
        size_t lineDepth = 0;
        size_t lineContent = 0;
        if (!quoteLine(lines[at], length, &lineDepth, &lineContent))
            break;
        if (lineDepth != depth)
            break;
        // A fence inside the quote is still a fence: strip the quote markers, shade
        // the same block band, and keep the quote gutter on every row. Callouts use
        // this path too, so a WARNING can contain a real code block.
        char fenceMarker = 0;
        size_t fenceRun = 0;
        size_t fenceOffset = 0;
        if (isFence(lines[at] + lineContent, length - lineContent,
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
        size_t end = trimRight(lines[at], length);
        if (!callout && end - lineContent >= 5u
            && lines[at][lineContent] == '[' && lines[at][lineContent + 1u] == '!') {
            size_t close = lineContent + 2u;
            while (close < end && lines[at][close] != ']')
                ++close;
            if (close < end && close > lineContent + 2u) {
                callout = true;
                if (!wrapStart(self, &wrap, true)
                    || !appendSegment(self, "! ", 2u, 1u,
                                      TERMINAL_MARKDOWN_MARKER, TERMINAL_STYLE_BOLD)
                    || !appendSegment(self, lines[at] + lineContent + 2u,
                                      close - lineContent - 2u, 1u,
                                      TERMINAL_MARKDOWN_MARKER, TERMINAL_STYLE_BOLD)
                    || !appendSegment(self, " !", 2u, 1u,
                                      TERMINAL_MARKDOWN_MARKER, TERMINAL_STYLE_BOLD))
                    return false;
                wrap.x += 2u + cellsOf(lines[at] + lineContent + 2u,
                                       close - lineContent - 2u) + 2u;
                wrap.content = true;
                if (!wrapBreak(self, &wrap))
                    return false;
                wrap.open = false;
                wrap.pendingSpace = false;
                if (close + 1u < end) {
                    if (!emitInline(self, &wrap, lines[at] + close + 1u,
                                    end - close - 1u, TERMINAL_MARKDOWN_BODY, 0, 0))
                        return false;
                }
                ++at;
                continue;
            }
        }
        if (end > lineContent) {
            wrapSpace(&wrap);
            if (!emitInline(self, &wrap, lines[at] + lineContent, end - lineContent,
                            callout ? TERMINAL_MARKDOWN_BODY : TERMINAL_MARKDOWN_QUOTE,
                            0, 0))
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

static MarkdownCellLine tableCellLine(const char *text, size_t length, size_t offset,
                                      size_t width) {
    MarkdownCellLine line = { .text = text + length, .next = length };
    while (offset < length && isSpace(text[offset]))
        ++offset;
    line.text = text + offset;
    if (offset == length)
        return line;
    size_t at = offset;
    size_t used = 0;
    size_t lastSpace = MARKDOWN_NO_INDEX;
    size_t afterSpace = MARKDOWN_NO_INDEX;
    while (at < length) {
        if (isSpace(text[at]) && at > offset) {
            lastSpace = at;
            afterSpace = at + 1u;
            while (afterSpace < length && isSpace(text[afterSpace]))
                ++afterSpace;
        }
        TerminalCanvasDecodedScalar decoded = TerminalCanvas_utf8Decode(text + at, length - at);
        size_t bytes = decoded.bytes == 0 ? 1u : decoded.bytes;
        size_t glyphCells = decoded.bytes == 0 ? 1u
                          : TerminalCanvas_widthOf(decoded.codepoint);
        if (glyphCells > width - (used < width ? used : width)) {
            if (lastSpace != MARKDOWN_NO_INDEX) {
                line.length = lastSpace - offset;
                line.next = afterSpace;
                line.cells = cellsOf(line.text, line.length);
                return line;
            }
            if (at == offset) {
                at += bytes;
                used += glyphCells;
            }
            line.length = at - offset;
            line.next = at;
            line.cells = used;
            return line;
        }
        used += glyphCells;
        at += bytes;
    }
    line.length = at - offset;
    line.next = length;
    line.cells = used;
    return line;
}

static size_t tableCellLineCount(const char *text, size_t length, size_t width) {
    size_t offset = 0;
    size_t count = 0;
    do {
        MarkdownCellLine line = tableCellLine(text, length, offset, width);
        ++count;
        if (line.next >= length || line.next <= offset)
            break;
        offset = line.next;
    } while (offset < length);
    return count;
}

static size_t tableColumnMinimum(const char *const *lines, size_t first, size_t end,
                                 size_t column) {
    const char *cells[MARKDOWN_MAX_COLUMNS];
    size_t lengths[MARKDOWN_MAX_COLUMNS];
    size_t count = 0;
    size_t minimum = 0;
    for (size_t row = first; row < end; ++row) {
        tableCells(lines[row], strlen(lines[row]), cells, lengths, &count);
        if (column >= count)
            continue;
        size_t at = 0;
        size_t wordCells = 0;
        while (at <= lengths[column]) {
            if (at == lengths[column] || isSpace(cells[column][at])) {
                if (wordCells > minimum)
                    minimum = wordCells;
                wordCells = 0;
                ++at;
                continue;
            }
            TerminalCanvasDecodedScalar decoded = TerminalCanvas_utf8Decode(
                cells[column] + at, lengths[column] - at);
            size_t bytes = decoded.bytes == 0 ? 1u : decoded.bytes;
            wordCells += decoded.bytes == 0 ? 1u
                         : TerminalCanvas_widthOf(decoded.codepoint);
            at += bytes;
        }
    }
    return minimum == 0u ? 1u : minimum;
}

static bool tableAllocateWidths(const size_t *needed, const size_t *minimums,
                                size_t *widths, size_t columns, size_t viewport) {
    size_t overhead = columns * 3u + 1u;
    if (viewport <= overhead)
        return false;
    size_t available = viewport - overhead;
    size_t neededTotal = 0;
    size_t minimumTotal = 0;
    bool neededFits = true;
    bool minimumsFit = true;
    for (size_t column = 0; column < columns; ++column) {
        if (needed[column] > available - neededTotal) {
            neededFits = false;
            neededTotal = available;
        } else if (neededFits) {
            neededTotal += needed[column];
        }
        if (minimums[column] > available - minimumTotal) {
            minimumsFit = false;
            minimumTotal = available;
        } else if (minimumsFit) {
            minimumTotal += minimums[column];
        }
        widths[column] = minimums[column];
    }
    if (!minimumsFit)
        return false;
    if (neededFits) {
        for (size_t column = 0; column < columns; ++column)
            widths[column] = needed[column];
        return true;
    }
    size_t extraBudget = available - minimumTotal;
    size_t remainingNeed = 0;
    for (size_t column = 0; column < columns; ++column) {
        size_t extra = needed[column] > minimums[column]
                     ? needed[column] - minimums[column] : 0u;
        if (extra > SIZE_MAX - remainingNeed)
            remainingNeed = SIZE_MAX;
        else
            remainingNeed += extra;
    }
    if (remainingNeed == 0u)
        return true;
    size_t used = 0;
    long double fractions[MARKDOWN_MAX_COLUMNS] = {0};
    for (size_t column = 0; column < columns; ++column) {
        size_t extra = needed[column] > minimums[column]
                     ? needed[column] - minimums[column] : 0u;
        long double share = (long double) extraBudget * (long double) extra
                          / (long double) remainingNeed;
        size_t whole = (size_t) share;
        if (whole > extra)
            whole = extra;
        widths[column] += whole;
        used += whole;
        fractions[column] = whole == extra ? -1.0L : share - (long double) whole;
    }
    while (used < extraBudget) {
        size_t chosen = MARKDOWN_NO_INDEX;
        for (size_t column = 0; column < columns; ++column)
            if (fractions[column] >= 0.0L
                && (chosen == MARKDOWN_NO_INDEX || fractions[column] > fractions[chosen]))
                chosen = column;
        if (chosen == MARKDOWN_NO_INDEX)
            break;
        ++widths[chosen];
        ++used;
        fractions[chosen] = -1.0L;
    }
    return true;
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
        || !appendSegment(self, left, strlen(left), 1u, TERMINAL_MARKDOWN_RULE, 0))
        return false;
    for (size_t column = 0; column < columns; ++column) {
        if (!appendSegment(self, MARKDOWN_BORDER_HORIZONTAL,
                           strlen(MARKDOWN_BORDER_HORIZONTAL), widths[column] + 2u,
                           TERMINAL_MARKDOWN_RULE, 0))
            return false;
        if (column + 1u < columns
            && !appendSegment(self, join, strlen(join), 1u, TERMINAL_MARKDOWN_RULE, 0))
            return false;
    }
    return appendSegment(self, right, strlen(right), 1u, TERMINAL_MARKDOWN_RULE, 0);
}

static bool appendTableSpaces(TerminalMarkdownView *self, size_t cells) {
    while (cells != 0u) {
        size_t chunk = cells > MARKDOWN_SPACES_CAP ? MARKDOWN_SPACES_CAP : cells;
        if (!appendSegment(self, MARKDOWN_SPACES, chunk, 1u,
                           TERMINAL_MARKDOWN_BODY, 0))
            return false;
        cells -= chunk;
    }
    return true;
}

static bool emitTableRow(TerminalMarkdownView *self, const char *const *cells,
                         const size_t *lengths, size_t count, size_t columns,
                         const size_t *widths, bool heading) {
    size_t offsets[MARKDOWN_MAX_COLUMNS] = {0};
    size_t heights[MARKDOWN_MAX_COLUMNS];
    for (size_t column = 0; column < columns; ++column)
        heights[column] = column < count
                        ? tableCellLineCount(cells[column], lengths[column], widths[column])
                        : 1u;
    size_t rowHeight = 1u;
    for (size_t column = 0; column < columns; ++column)
        if (heights[column] > rowHeight)
            rowHeight = heights[column];
    for (size_t visualRow = 0; visualRow < rowHeight; ++visualRow) {
        if (!appendRow(self)
            || !appendSegment(self, MARKDOWN_BORDER_VERTICAL,
                              strlen(MARKDOWN_BORDER_VERTICAL), 1u,
                              TERMINAL_MARKDOWN_RULE, 0)
            || !appendSegment(self, " ", 1u, 1u, TERMINAL_MARKDOWN_BODY, 0))
            return false;
        for (size_t column = 0; column < columns; ++column) {
            MarkdownCellLine line = {0};
            if (visualRow < heights[column]) {
                const char *cell = column < count ? cells[column] : "";
                size_t length = column < count ? lengths[column] : 0u;
                line = tableCellLine(cell, length, offsets[column], widths[column]);
                offsets[column] = line.next;
                if (!appendSegment(self, line.text, line.length, 1u,
                                   heading ? TERMINAL_MARKDOWN_HEADING
                                           : TERMINAL_MARKDOWN_BODY, 0))
                    return false;
            }
            if (line.cells < widths[column]
                && !appendTableSpaces(self, widths[column] - line.cells))
                return false;
            if (column + 1u < columns) {
                if (!appendSegment(self, " ", 1u, 1u, TERMINAL_MARKDOWN_BODY, 0)
                    || !appendSegment(self, MARKDOWN_BORDER_VERTICAL,
                                      strlen(MARKDOWN_BORDER_VERTICAL), 1u,
                                      TERMINAL_MARKDOWN_RULE, 0)
                    || !appendSegment(self, " ", 1u, 1u, TERMINAL_MARKDOWN_BODY, 0))
                    return false;
            }
        }
        if (!appendSegment(self, " ", 1u, 1u, TERMINAL_MARKDOWN_BODY, 0)
            || !appendSegment(self, MARKDOWN_BORDER_VERTICAL,
                              strlen(MARKDOWN_BORDER_VERTICAL), 1u,
                              TERMINAL_MARKDOWN_RULE, 0))
            return false;
    }
    return true;
}

static bool emitTableCompact(TerminalMarkdownView *self, const char *const *lines,
                             size_t header, size_t end, size_t columns) {
    const char *headers[MARKDOWN_MAX_COLUMNS];
    size_t headerLengths[MARKDOWN_MAX_COLUMNS];
    size_t headerCount = 0;
    tableCells(lines[header], strlen(lines[header]), headers, headerLengths, &headerCount);
    if (!emitGap(self))
        return false;
    for (size_t row = header + 2u; row < end; ++row) {
        const char *cells[MARKDOWN_MAX_COLUMNS];
        size_t lengths[MARKDOWN_MAX_COLUMNS];
        size_t count = 0;
        tableCells(lines[row], strlen(lines[row]), cells, lengths, &count);
        for (size_t column = 0; column < columns; ++column) {
            MarkdownPrefix prefix = {0};
            MarkdownWrap wrap = { .prefix = &prefix, .width = (*self).width };
            if (column < headerCount && headerLengths[column] != 0u) {
                if (!emitInline(self, &wrap, headers[column], headerLengths[column],
                                TERMINAL_MARKDOWN_HEADING, 0, 0))
                    return false;
                wrapSpace(&wrap);
                if (!appendPlain(self, &wrap, ":", 1u, TERMINAL_MARKDOWN_BODY, 0))
                    return false;
                wrapSpace(&wrap);
            }
            if (column < count && lengths[column] != 0u) {
                if (!emitInline(self, &wrap, cells[column], lengths[column],
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
    if (columns > MARKDOWN_MAX_COLUMNS)
        columns = MARKDOWN_MAX_COLUMNS;
    size_t widths[MARKDOWN_MAX_COLUMNS] = {0};
    size_t needed[MARKDOWN_MAX_COLUMNS] = {0};
    size_t minimums[MARKDOWN_MAX_COLUMNS];
    size_t header = *index;
    size_t body = header + 2u;
    size_t end = body;
    const char *cells[MARKDOWN_MAX_COLUMNS];
    size_t lengths[MARKDOWN_MAX_COLUMNS];
    size_t count = 0;
    tableCells(lines[header], strlen(lines[header]), cells, lengths, &count);
    for (size_t column = 0; column < columns && column < count; ++column) {
        size_t width = cellsOf(cells[column], lengths[column]);
        if (width > needed[column])
            needed[column] = width;
    }
    while (end < lineCount) {
        size_t length = strlen(lines[end]);
        if (lineBlank(lines[end], length)
            || findBytes(lines[end], length, 0, "|", 1u) == MARKDOWN_NO_INDEX)
            break;
        tableCells(lines[end], length, cells, lengths, &count);
        for (size_t column = 0; column < columns && column < count; ++column) {
            size_t width = cellsOf(cells[column], lengths[column]);
            if (width > needed[column])
                needed[column] = width;
        }
        ++end;
    }
    for (size_t column = 0; column < columns; ++column) {
        if (needed[column] == 0u)
            needed[column] = 1u;
        minimums[column] = tableColumnMinimum(lines, header, end, column);
    }
    if (!tableAllocateWidths(needed, minimums, widths, columns, (*self).width)) {
        if (!emitTableCompact(self, lines, header, end, columns))
            return false;
        *index = end;
        return true;
    }
    if (!emitGap(self))
        return false;
    if (!emitTableBorder(self, widths, columns, 0u))
        return false;
    tableCells(lines[header], strlen(lines[header]), cells, lengths, &count);
    if (!emitTableRow(self, cells, lengths, count, columns, widths, true)
        || !emitTableBorder(self, widths, columns, 1u))
        return false;
    for (size_t row = body; row < end; ++row) {
        tableCells(lines[row], strlen(lines[row]), cells, lengths, &count);
        if (!emitTableRow(self, cells, lengths, count, columns, widths, false))
            return false;
        if (!emitTableBorder(self, widths, columns, row + 1u < end ? 1u : 2u))
            return false;
    }
    if (body == end && !emitTableBorder(self, widths, columns, 2u))
        return false;
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
    (*self).codeBlockCount = 0;
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
    TerminalMarkdownStyle *style = &(*self).style;
    (*style).body = TerminalPen_0();
    (*style).heading = TerminalPen_1(TERMINAL_STYLE_BOLD);
    (*style).code = TerminalPen_2(0, TERMINAL_RGB(112, 190, 130));
    (*style).link = TerminalPen_1(TERMINAL_STYLE_UNDERLINE);
    (*style).quote = TerminalPen_2(TERMINAL_STYLE_BOLD, TERMINAL_RGB(105, 190, 220));
    (*style).rule = TerminalPen_1(TERMINAL_STYLE_DIM);
    (*style).marker = TerminalPen_2(TERMINAL_STYLE_BOLD, TERMINAL_RGB(235, 190, 95));
    (*style).fence =
        TerminalPen_3(TERMINAL_STYLE_DIM, TERMINAL_COLOR_DEFAULT, MARKDOWN_CODE_BLOCK_BG);
    return self;
}

void TerminalMarkdownView_free(TerminalMarkdownView *self) {
    if (self == nullptr)
        return;
    Memory_free((*self).rows);
    Memory_free((*self).segments);
    Memory_free((*self).codeBlocks);
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
    (*self).codeBlockCount = 0;
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
    (*self).codeBlockCount = 0;
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
    return self != nullptr ? (*self).codeBlockCount : 0u;
}

TerminalMarkdownCodeBlock TerminalMarkdownView_codeBlockAt(const TerminalMarkdownView *self,
                                                           size_t row) {
    TerminalMarkdownCodeBlock none = {0};
    if (self == nullptr)
        return none;
    size_t low = 0;
    size_t high = (*self).codeBlockCount;
    while (low < high) {
        size_t mid = low + (high - low) / 2u;
        const MarkdownCodeBlockRecord *block = &(*self).codeBlocks[mid];
        if (row < (*block).firstRow) {
            high = mid;
            continue;
        }
        if (row - (*block).firstRow >= (*block).rowCount) {
            low = mid + 1u;
            continue;
        }
        return (TerminalMarkdownCodeBlock) {
            true, (*block).firstRow, (*block).rowCount, (*block).firstSourceLine,
            (*block).sourceLineCount, (*block).quoteDepth
        };
    }
    return none;
}

TerminalMarkdownCodeLine TerminalMarkdownView_codeBlockLine(const TerminalMarkdownView *self,
                                                            TerminalMarkdownCodeBlock block,
                                                            size_t line) {
    TerminalMarkdownCodeLine none = {0};
    if (self == nullptr || !block.valid || line >= block.sourceLineCount)
        return none;
    if (block.firstSourceLine > SIZE_MAX - line)
        return none;
    size_t sourceLine = block.firstSourceLine + line;
    if (sourceLine >= (*self).lineCount)
        return none;
    const char *text = nullptr;
    size_t length = 0;
    if (!fenceContentText((*self).lines[sourceLine], block.quoteDepth, &text, &length))
        return none;
    return (TerminalMarkdownCodeLine) { true, text, length };
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
        if ((*record).codeBlock) {
            // The wash is painted UNDER the text so code glyph cells keep their own
            // foreground while every cell of the row carries the block background.
            TerminalPen block = penForRole(&(*self).style, TERMINAL_MARKDOWN_FENCE);
            block.style = 0;
            block.fg = TERMINAL_COLOR_DEFAULT;
            TerminalCanvas_hline(canvas, row, ' ', block);
        }
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
        "segmentCapacity=%zu,codeBlocks=<owned:%zu>,codeBlockCount=%zu,"
        "codeBlockCapacity=%zu,style=<pens>,refusals=%zu}",
        (*self).lineCount, (*self).lineCount, (*self).dirty, (*self).ready,
        (*self).truncated, (*self).failed, (*self).width, (*self).visibleRows,
        (*self).scroll, (*self).rowCapacity, (*self).rowCount, (*self).rowCapacity,
        (*self).segmentCapacity, (*self).segmentCount, (*self).segmentCapacity,
        (*self).codeBlockCapacity, (*self).codeBlockCount, (*self).codeBlockCapacity,
        (*self).refusals);
}
