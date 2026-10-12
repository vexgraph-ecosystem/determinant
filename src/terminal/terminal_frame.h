#ifndef DETERMINANT_TERMINAL_FRAME_H
#define DETERMINANT_TERMINAL_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "terminal/terminal_canvas.h"
#include "terminal/terminal_image.h"
#include "c23/constructor.h"

// terminal/terminal_frame.h — a bare region layout, no application semantics.
//
// TerminalFrame divides a rectangle into named REGIONS and hands each back as a
// TerminalCanvas: an optional header band, a body split into left / centre /
// right columns, and an optional footer band under the centre column. It knows
// nothing about chats, channels, users, settings or badges — the caller draws
// whatever it wants into whichever region. That is the whole point: the toolkit
// owns geometry, the application owns content and policy.
//
// It owns no memory and no device. It may paint one borrowed BACKGROUND IMAGE
// behind the regions. All geometry is recomputed from the canvas size on every
// call, so a resize is free.

#define TERMINAL_FRAME_HEADER_ROWS_DEFAULT 2u   // one title row + one rule row

typedef struct TerminalFrame {
    size_t leftWidth;    // requested left-column width
    size_t rightWidth;   // requested right-column width
    size_t headerRows;   // top band rows (0 => none); the rule sits on the last
    size_t footerRows;   // bottom band rows under the centre column (0 => none)
    const TerminalImage *background;   // borrowed, nullable
} TerminalFrame;

TerminalFrame TerminalFrame_0(void);
TerminalFrame TerminalFrame_2(size_t leftWidth, size_t rightWidth);
#define TerminalFrame(...) CONSTRUCTOR_DISPATCH(TerminalFrame, __VA_ARGS__)
#define TerminalFrame_zero() TerminalFrame_0()

bool TerminalFrame_setRails(TerminalFrame *self, size_t leftWidth, size_t rightWidth);
void TerminalFrame_getRails(const TerminalFrame *self, size_t *outLeft, size_t *outRight);
bool TerminalFrame_setBands(TerminalFrame *self, size_t headerRows, size_t footerRows);
void TerminalFrame_getBands(const TerminalFrame *self, size_t *outHeader, size_t *outFooter);
bool TerminalFrame_setBackground(TerminalFrame *self, const TerminalImage *image);
const TerminalImage *TerminalFrame_getBackground(const TerminalFrame *self);
// Rows/columns actually used, given a root size (rails may collapse to zero on a
// narrow terminal so the centre keeps at least one column).
void TerminalFrame_getLayout(const TerminalFrame *self, const TerminalCanvas *root,
                             size_t *outLeft, size_t *outRight, bool *outRails);

// Paint the borrowed background image across the whole root (no-op when none).
void TerminalFrame_paintBackground(const TerminalFrame *self, TerminalCanvas *root);

// --- Regions (all clipped; invalid when the root is too small) ---
TerminalCanvas TerminalFrame_header(const TerminalFrame *self, const TerminalCanvas *root);
TerminalCanvas TerminalFrame_left(const TerminalFrame *self, const TerminalCanvas *root);
TerminalCanvas TerminalFrame_center(const TerminalFrame *self, const TerminalCanvas *root);
TerminalCanvas TerminalFrame_right(const TerminalFrame *self, const TerminalCanvas *root);
TerminalCanvas TerminalFrame_footer(const TerminalFrame *self, const TerminalCanvas *root);

// Draw the frame's own rules: the header rule and the two column separators.
void TerminalFrame_drawRules(const TerminalFrame *self, TerminalCanvas *root, TerminalPen rule);

bool TerminalFrame_toString(const TerminalFrame *self, char *dest, size_t cap, bool *outTruncated);
bool TerminalFrame_toStringStruct(const TerminalFrame *self, char *dest, size_t cap, bool *outTruncated);

#endif
