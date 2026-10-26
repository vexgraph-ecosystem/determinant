#ifndef DETERMINANT_TERMINAL_MARKDOWN_TABLE_H
#define DETERMINANT_TERMINAL_MARKDOWN_TABLE_H

#include <stdbool.h>
#include <stddef.h>

// ui/markdown/markdown_table.h — the pipe-table model.
//
// A GFM table is three things the old renderer conflated: a COLUMN ALIGNMENT
// declaration in the delimiter row, a splitter that knows `\|` and inline code
// are not cell boundaries, and a width policy (natural width, longest-word
// minimum, then honest truncation with an ellipsis). This owner answers those
// three questions without owning a pen or a row; the view decides where the box
// is drawn.

#define TERMINAL_MARKDOWN_TABLE_COLUMNS 16u

typedef enum TerminalMarkdownTableAlign {
    TERMINAL_MARKDOWN_TABLE_LEFT = 0,
    TERMINAL_MARKDOWN_TABLE_CENTER,
    TERMINAL_MARKDOWN_TABLE_RIGHT
} TerminalMarkdownTableAlign;

// VALUE RECORD: TerminalMarkdownTableCellLine (no behaviour; borrowed view).
// One wrapped line of a cell. `next` is the source byte offset the next line
// starts at, so a caller can walk a cell without re-measuring. `truncated` means
// the line was cut inside a word and the caller should append an ellipsis (the
// line leaves one cell free for it).
typedef struct TerminalMarkdownTableCellLine {
    const char *text;
    size_t length;
    size_t next;
    size_t cells;
    bool truncated;
} TerminalMarkdownTableCellLine;

// Parse a delimiter row (`| :--- | :---: | ---: |`) into per-column alignment.
// Returns the column count (>= 2), or 0 when the row is not a delimiter. A null
// `aligns` only counts.
size_t TerminalMarkdownTable_parseDelimiter(const char *text, size_t length,
                                    TerminalMarkdownTableAlign *aligns, size_t capacity);

// Split one row into trimmed cell slices, honouring `\|` and backtick code spans
// so a literal pipe cannot invent a column. An optional leading/trailing pipe is
// ignored. Returns the cell count; never writes more than `capacity`.
size_t TerminalMarkdownTable_split(const char *text, size_t length, const char **cells,
                           size_t *lengths, size_t capacity);

// Display width of the whole cell (natural width).
size_t TerminalMarkdownTable_natural(const char *text, size_t length);

// Display width of the longest word in the cell, floored at 1 — the width below
// which the cell would have to break inside a word.
size_t TerminalMarkdownTable_longestWord(const char *text, size_t length);

// Number of wrapped display lines the cell occupies at `width`.
size_t TerminalMarkdownTable_lineCount(const char *text, size_t length, size_t width);

// One wrapped line starting at source offset `offset` within a cell of `width`.
// Breaks between words; when a single word cannot fit it cuts at `width - 1`
// cells and sets `truncated`, leaving the ellipsis cell to the caller.
TerminalMarkdownTableCellLine TerminalMarkdownTable_cellLine(const char *text, size_t length,
                                             size_t offset, size_t width);

// Allocate a viewport across columns: natural widths when they fit, otherwise
// minimums plus a proportional share of the slack. False when even the minimums
// do not fit, so the caller can pick a different presentation instead of
// clipping a column to zero.
bool TerminalMarkdownTable_allocate(const size_t *needed, const size_t *minimums, size_t *widths,
                            size_t columns, size_t viewport);

#endif
