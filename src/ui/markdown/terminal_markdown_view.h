#ifndef DETERMINANT_TERMINAL_MARKDOWN_VIEW_H
#define DETERMINANT_TERMINAL_MARKDOWN_VIEW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ui/surface/terminal_canvas.h"
#include "ui/surface/terminal_projection.h"
#include "ui/markdown/markdown_theme.h"
#include "ui/markdown/markdown_code_block.h"
#include "ui/markdown/markdown_action.h"
#include "c23/constructor.h"

// ui/markdown/terminal_markdown_view.h — rendered Markdown in terminal cells.
//
// This is a READ-ONLY document view, not a CommonMark implementation. It owns a
// GFM-ish subset — headings, paragraphs, emphasis, inline code, links, lists,
// task items, quotes/callouts, shaded fenced code, thematic breaks and bordered
// simple tables with wrapped cells — and deliberately leaves HTML layout, images,
// footnotes, math and nested tables to the caller or to a later slice. A fenced
// code block is a full-width translucent background band (no box glyphs) with one
// blank row above and below it; the same treatment applies inside a callout. The
// subset is stated so nobody mistakes a preview for a conformance claim.
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
// Ordered-list items align to the widest marker in their sibling list. Marker
// width is measured from source text, not a fixed-width integer or digit limit.

#define TERMINAL_MARKDOWN_ROWS_LIMIT 100000u
#define TERMINAL_MARKDOWN_ROWS_DEFAULT 64u
#define TERMINAL_MARKDOWN_SEGMENTS_DEFAULT 256u

typedef struct TerminalMarkdownView TerminalMarkdownView;

// The theme is the full pen ladder; setStyle is its legacy projection. Setting
// either keeps the other in sync, so a caller that only knows the eight-pen
// record still gets per-level headings from the default ladder until it chooses
// a colour.
bool TerminalMarkdownView_setTheme(TerminalMarkdownView *self, TerminalMarkdownTheme theme);
TerminalMarkdownTheme TerminalMarkdownView_getTheme(const TerminalMarkdownView *self);

// Hit testing is COLD and in DOCUMENT rows (the same space as codeBlockAt), not
// screen rows. `actionAt` says what a press on that cell means; the view never
// performs it. `setHover` moves the link highlight; `clearHover` removes it.
TerminalMarkdownAction TerminalMarkdownView_actionAt(const TerminalMarkdownView *self,
                                                     size_t row, size_t column);
bool TerminalMarkdownView_setHover(TerminalMarkdownView *self, size_t row, size_t column);
void TerminalMarkdownView_clearHover(TerminalMarkdownView *self);

// Copy the fence at `block` into `dest` (newline-separated, no trailing
// newline). Atomic and allocation-free; false when it does not fit. The caller
// then performs the clipboard write and calls setCopied for the check glyph.
bool TerminalMarkdownView_copyBlock(const TerminalMarkdownView *self, size_t block,
                                    char *dest, size_t capacity, size_t *written);
bool TerminalMarkdownView_setCopied(TerminalMarkdownView *self, size_t block, bool copied);
bool TerminalMarkdownView_isCopied(const TerminalMarkdownView *self, size_t block);

size_t TerminalMarkdownView_codeBlockCount(const TerminalMarkdownView *self);
// The block whose shaded rows cover `row` (a document row, not a screen row), or
// an invalid record when that row is not inside a code block. Cold linear/binary
// lookup; callers may use it per hover frame.
TerminalMarkdownCodeBlock TerminalMarkdownView_codeBlockAt(const TerminalMarkdownView *self,
                                                           size_t row);
// One source line of `block` (0..sourceLineCount-1), borrowed from the view's
// source and with quote markers removed. An invalid block/index returns invalid.
TerminalMarkdownCodeLine TerminalMarkdownView_codeBlockLine(const TerminalMarkdownView *self,
                                                            TerminalMarkdownCodeBlock block,
                                                            size_t line);

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
