#ifndef DETERMINANT_MARKDOWN_CODE_BLOCK_H
#define DETERMINANT_MARKDOWN_CODE_BLOCK_H

#include <stdbool.h>
#include <stddef.h>

#include "ui/surface/terminal_projection.h"
#include "c23/constructor.h"

// ui/markdown/markdown_code_block.h — laid-out fences and their copy projection.
//
// A fenced block has two lives. On screen it is a shaded document range; for the
// clipboard it is the source lines between the fences, in order, with the quote
// gutters stripped when the fence sits inside a callout. Both are properties of
// the same record, so the copy affordance cannot disagree with the band it is
// drawn on. The pool owns the records and grows cold; the source lines stay
// borrowed from the document.

// VALUE RECORD: TerminalMarkdownCodeBlock (no behaviour; returned by value).
// `firstRow`/`rowCount` name the shaded document rows, while
// `firstSourceLine`/`sourceLineCount` name its content lines in the borrowed
// source (the opening and closing fences are excluded). `quoteDepth` is nonzero
// when the fence sits inside a blockquote/callout, so the copy projection strips
// the quote markers from each line.
typedef struct TerminalMarkdownCodeBlock {
    bool valid;
    size_t firstRow;
    size_t rowCount;
    size_t firstSourceLine;
    size_t sourceLineCount;
    size_t quoteDepth;
} TerminalMarkdownCodeBlock;

// VALUE RECORD: TerminalMarkdownCodeLine (no behaviour; borrowed view).
typedef struct TerminalMarkdownCodeLine {
    bool valid;
    const char *text;
    size_t length;
} TerminalMarkdownCodeLine;

typedef struct TerminalMarkdownCodeBlockPool TerminalMarkdownCodeBlockPool;

TerminalMarkdownCodeBlockPool *TerminalMarkdownCodeBlockPool_0(void);
#define TerminalMarkdownCodeBlockPool(...) \
    CONSTRUCTOR_DISPATCH(TerminalMarkdownCodeBlockPool, __VA_ARGS__)
#define TerminalMarkdownCodeBlockPool_zero() TerminalMarkdownCodeBlockPool_0()

void TerminalMarkdownCodeBlockPool_free(TerminalMarkdownCodeBlockPool *self);

// Drop every record but keep the admitted capacity: a re-layout reuses the pool.
void TerminalMarkdownCodeBlockPool_reset(TerminalMarkdownCodeBlockPool *self);

// Record one fence. A zero-row block is ignored (an empty fence still owns a
// source range but has nothing to shade); the call is refused only when the cold
// growth is refused, and the refusal count is queryable.
bool TerminalMarkdownCodeBlockPool_add(TerminalMarkdownCodeBlockPool *self,
                                       size_t firstRow, size_t rowCount,
                                       size_t firstSourceLine, size_t sourceLineCount,
                                       size_t quoteDepth);

size_t TerminalMarkdownCodeBlockPool_count(const TerminalMarkdownCodeBlockPool *self);
size_t TerminalMarkdownCodeBlockPool_exhaustionCount(const TerminalMarkdownCodeBlockPool *self);

// The record whose shaded rows cover `row`, and its index (nullable). Cold.
bool TerminalMarkdownCodeBlockPool_indexAt(const TerminalMarkdownCodeBlockPool *self,
                                           size_t row, size_t *index);
TerminalMarkdownCodeBlock TerminalMarkdownCodeBlockPool_at(const TerminalMarkdownCodeBlockPool *self,
                                                           size_t index);

// One source line of a RECORD (the view keeps the record a caller was given, so
// it can resolve a line without searching the pool). Invalid line -> invalid.
TerminalMarkdownCodeLine TerminalMarkdownCodeBlock_lineOf(const char *const *lines,
                                                          size_t lineCount,
                                                          TerminalMarkdownCodeBlock block,
                                                          size_t line);

// One source line of the fence at `index`, quote markers stripped. Invalid
// block/line returns an invalid record.
TerminalMarkdownCodeLine TerminalMarkdownCodeBlockPool_line(const TerminalMarkdownCodeBlockPool *self,
                                                            const char *const *lines,
                                                            size_t lineCount, size_t index,
                                                            size_t line);

// Copy the fence's source lines into `dest`, newline-separated with no trailing
// newline. Atomic: returns false and writes nothing when the content plus the
// terminator does not fit. `written` receives the byte count (nullable).
bool TerminalMarkdownCodeBlockPool_copy(const TerminalMarkdownCodeBlockPool *self,
                                        const char *const *lines, size_t lineCount,
                                        size_t index, char *dest, size_t capacity,
                                        size_t *written);

TerminalProjection TerminalMarkdownCodeBlockPool_toString(const TerminalMarkdownCodeBlockPool *self);
TerminalProjection TerminalMarkdownCodeBlockPool_toStringStruct(const TerminalMarkdownCodeBlockPool *self);

#endif
