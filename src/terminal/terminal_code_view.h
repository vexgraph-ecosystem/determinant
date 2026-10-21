#ifndef DETERMINANT_TERMINAL_CODE_VIEW_H
#define DETERMINANT_TERMINAL_CODE_VIEW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "terminal/terminal_canvas.h"
#include "terminal/terminal_projection.h"
#include "c23/constructor.h"

// A code line is borrowed text plus caller-produced byte spans. Determinant
// owns cell clipping and painting only: it does not tokenize or infer meaning.
// Byte offsets always refer to the original UTF-8 source, never to virtual hints.

typedef struct TerminalCodeSpan {
    size_t startByte;
    size_t endByte;
    TerminalPen pen;
} TerminalCodeSpan;

typedef struct TerminalCodeHint {
    size_t byteOffset;
    const char *text;
    size_t length;
    TerminalPen pen;
} TerminalCodeHint;

typedef struct TerminalCodeView TerminalCodeView;

TerminalCodeView *TerminalCodeView_0(void);
#define TerminalCodeView(...) CONSTRUCTOR_DISPATCH(TerminalCodeView, __VA_ARGS__)
#define TerminalCodeView_zero() TerminalCodeView_0()

void TerminalCodeView_free(TerminalCodeView *self);

// Borrow one UTF-8 line and its ordered, non-overlapping byte spans and sorted
// virtual hints. A hint is painted immediately before its source byte offset;
// it never enters the source, editing model, or span coordinates. Invalid input
// rejects without replacing the current line. Null arrays are valid only at
// count zero. The caller keeps all borrowed values alive until the next update.
bool TerminalCodeView_setLine(TerminalCodeView *self, const char *text, size_t length,
                              const TerminalCodeSpan *spans, size_t spanCount,
                              const TerminalCodeHint *hints, size_t hintCount);
const char *TerminalCodeView_getText(const TerminalCodeView *self);
size_t TerminalCodeView_getLength(const TerminalCodeView *self);
const TerminalCodeSpan *TerminalCodeView_getSpans(const TerminalCodeView *self);
size_t TerminalCodeView_getSpanCount(const TerminalCodeView *self);
const TerminalCodeHint *TerminalCodeView_getHints(const TerminalCodeView *self);
size_t TerminalCodeView_getHintCount(const TerminalCodeView *self);

// Horizontal offset and clipping use terminal cells. Rendering is hot,
// allocation-free and quiet. x/y are local to the canvas.
bool TerminalCodeView_setScroll(TerminalCodeView *self, size_t cells);
size_t TerminalCodeView_getScroll(const TerminalCodeView *self);
void TerminalCodeView_render(const TerminalCodeView *self, TerminalCanvas *canvas,
                             size_t x, size_t y);

TerminalProjection TerminalCodeView_toString(const TerminalCodeView *self);
TerminalProjection TerminalCodeView_toStringStruct(const TerminalCodeView *self);

#endif
