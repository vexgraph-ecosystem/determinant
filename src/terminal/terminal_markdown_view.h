#ifndef DETERMINANT_TERMINAL_MARKDOWN_VIEW_H
#define DETERMINANT_TERMINAL_MARKDOWN_VIEW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "terminal/terminal_canvas.h"
#include "terminal/terminal_projection.h"
#include "c23/constructor.h"

// terminal/terminal_markdown_view.h — rendered Markdown in terminal cells.
//
// This is a READ-ONLY document view, not a CommonMark implementation. It owns a
// GFM-ish subset — headings, paragraphs, emphasis, inline code, links, lists,
// task items, quotes/callouts, fenced code, thematic breaks and bordered simple
// tables — and deliberately leaves HTML layout, images, footnotes, math and nested tables to
// the caller or to a later slice. The subset is stated so nobody mistakes a
// preview for a conformance claim.
//
// Source lines are BORROWED. The caller keeps them alive for the life of the
// source and calls setSource(nullptr, 0) before freeing them. Layout is COLD:
// setViewport(width) parses and wraps; render() only walks the cached rows and
// never allocates, exactly as a frame path requires. The style record is resolved
// at render time, so recolouring the document does not reflow it.
//
// Rows are flat: a row is a range of segments in one pool, so wrapping thousands
// of words costs one growable array rather than one allocation per row. A blank
// row is a row with no segments. The row count includes blank rows, because the
// caller scrolls the document, not the source.

#define TERMINAL_MARKDOWN_ROWS_LIMIT 100000u
#define TERMINAL_MARKDOWN_ROWS_DEFAULT 64u
#define TERMINAL_MARKDOWN_SEGMENTS_DEFAULT 256u

// Nine roles, one pen each. A role with an extra style flag (BOLD or UNDERLINE)
// is how strong/emphasis combine without exploding the role set: the base role
// supplies the colour, the flag supplies the weight.
typedef enum TerminalMarkdownRole {
    TERMINAL_MARKDOWN_BODY = 0,
    TERMINAL_MARKDOWN_HEADING,
    TERMINAL_MARKDOWN_CODE,
    TERMINAL_MARKDOWN_LINK,
    TERMINAL_MARKDOWN_QUOTE,
    TERMINAL_MARKDOWN_RULE,
    TERMINAL_MARKDOWN_MARKER,
    TERMINAL_MARKDOWN_FENCE
} TerminalMarkdownRole;

typedef struct TerminalMarkdownStyle {
    TerminalPen body;
    TerminalPen heading;
    TerminalPen code;
    TerminalPen link;
    TerminalPen quote;
    TerminalPen rule;
    TerminalPen marker;
    TerminalPen fence;
} TerminalMarkdownStyle;

typedef struct TerminalMarkdownView TerminalMarkdownView;

TerminalMarkdownView *TerminalMarkdownView_0(void);
#define TerminalMarkdownView(...) CONSTRUCTOR_DISPATCH(TerminalMarkdownView, __VA_ARGS__)
#define TerminalMarkdownView_zero() TerminalMarkdownView_0()

// Frees the view and its row/segment pools. The borrowed source is never freed.
void TerminalMarkdownView_free(TerminalMarkdownView *self);

// Borrow `count` non-null lines. A null list or null line with a nonzero count is refused. The source
// stays borrowed: the caller must clear it before freeing the lines. Marking the
// view dirty is enough; the next setViewport lays it out at the then-current width.
bool TerminalMarkdownView_setSource(TerminalMarkdownView *self, const char *const *lines, size_t count);

// Cold layout: parse and wrap the borrowed source at `width` content cells. A
// width of zero is refused. Re-laying out at a new width keeps the scroll row,
// clamped to the new document; the caller resets the scroll when it wants the top.
bool TerminalMarkdownView_setViewport(TerminalMarkdownView *self, size_t width);

size_t TerminalMarkdownView_rowCount(const TerminalMarkdownView *self);
size_t TerminalMarkdownView_getWidth(const TerminalMarkdownView *self);

// The scroll is in WRAPPED ROWS, clamped to [0, maxScroll]. `visibleRows` is what
// the caller can show; setVisibleRows is trivial so a caller may call it per frame.
bool TerminalMarkdownView_setVisibleRows(TerminalMarkdownView *self, size_t rows);
size_t TerminalMarkdownView_visibleRows(const TerminalMarkdownView *self);
size_t TerminalMarkdownView_maxScroll(const TerminalMarkdownView *self);
bool TerminalMarkdownView_setScroll(TerminalMarkdownView *self, size_t row);
size_t TerminalMarkdownView_getScroll(const TerminalMarkdownView *self);
bool TerminalMarkdownView_scrollBy(TerminalMarkdownView *self, long delta);

bool TerminalMarkdownView_isReady(const TerminalMarkdownView *self);
bool TerminalMarkdownView_isTruncated(const TerminalMarkdownView *self);
size_t TerminalMarkdownView_exhaustionCount(const TerminalMarkdownView *self);
void TerminalMarkdownView_clearExhaustion(TerminalMarkdownView *self);

bool TerminalMarkdownView_setStyle(TerminalMarkdownView *self, TerminalMarkdownStyle style);
TerminalMarkdownStyle TerminalMarkdownView_getStyle(const TerminalMarkdownView *self);

// Hot: draw the rows visible from the current scroll into `canvas`, row 0 first.
// Clipped to the canvas, allocation-free and quiet. A null view, an invalid
// canvas, or a view that is not ready is a no-op.
void TerminalMarkdownView_render(const TerminalMarkdownView *self, TerminalCanvas *canvas);

TerminalProjection TerminalMarkdownView_toString(const TerminalMarkdownView *self);
TerminalProjection TerminalMarkdownView_toStringStruct(const TerminalMarkdownView *self);

#endif
