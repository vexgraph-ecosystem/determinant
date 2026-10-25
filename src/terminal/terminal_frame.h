#ifndef DETERMINANT_TERMINAL_FRAME_H
#define DETERMINANT_TERMINAL_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ui/surface/terminal_canvas.h"
#include "ui/surface/terminal_projection.h"
#include "ui/image/terminal_image.h"
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

// A rule glyph set. Glyphs are CODEPOINTS, so an appearance can pick ASCII
// ('-', '|') or real box drawing ('─', '│', '┼'). The defaults are a CONNECTOR:
// the horizontal run is joined to the vertical rails by junction glyphs (like the
// `|`/`-`/`` ` `` leaders in type.h), and the run may be STRIPED so it reads as a
// dotted guide rather than a solid `------`.
//
// DETACHED boxes: a panel is drawn as its OWN box with real corners
// ('┌','┐','└','┘') instead of meeting every neighbour at a junction — that is
// the visual language of a terminal instrument, not one continuous table.
// A rule drawn between panels is CUT: it stops short of the panel's verticals
// and may carry cut-endpoint glyphs ('╶','╴') so the line visibly ends.
typedef struct TerminalRule {
    uint32_t horizontal;  // horizontal run glyph (default '-')
    uint32_t vertical;    // rail glyph (default '|')
    uint32_t junction;    // where a rail meets a horizontal run (default '+')
    uint32_t corner;      // rail top when there is no header run (default '+')
    uint32_t stripe;      // alternate run glyph; 0 means no striping
    size_t stripeEvery;   // period of the stripe (e.g. 4 => "---.")
    // Detached-box corners; 0 falls back to `corner`.
    uint32_t cornerTopLeft, cornerTopRight, cornerBottomLeft, cornerBottomRight;
    // CUT endpoints for a rule that stops short (0 => a plain space).
    uint32_t cutLeft, cutRight;
} TerminalRule;
typedef struct TerminalFramePair { size_t first; size_t second; } TerminalFramePair;
typedef struct TerminalFrameLayout { size_t left; size_t right; bool rails; } TerminalFrameLayout;

TerminalRule TerminalRule_0(void);                                    // connector, solid ASCII
TerminalRule TerminalRule_2(uint32_t horizontal, uint32_t vertical);  // custom glyphs
TerminalRule TerminalRule_4(uint32_t horizontal, uint32_t vertical, uint32_t stripe, size_t stripeEvery);
#define TerminalRule(...) CONSTRUCTOR_DISPATCH(TerminalRule, __VA_ARGS__)
#define TerminalRule_zero() TerminalRule_0()

// Draw one styled horizontal run into a canvas region row (app footer rules).
void TerminalRule_draw(const TerminalRule *rule, TerminalCanvas *canvas, size_t ly, TerminalPen pen);
// Draw ONE horizontal run that is CUT: inset from both edges of the region and
// terminated with the cut-endpoint glyphs, so it stops short the way a panel
// divider does. `inset` is the blank margin inside each edge (0 => touching).
void TerminalRule_drawCut(const TerminalRule *rule, TerminalCanvas *canvas, size_t ly,
                          size_t inset, TerminalPen pen);
// Draw a DETACHED box around the whole canvas region: its own corners, its own
// verticals, never joined to a neighbour.
void TerminalRule_drawBox(const TerminalRule *rule, TerminalCanvas *canvas, TerminalPen pen);

typedef struct TerminalFrame {
    size_t leftWidth;    // requested left-column width
    size_t rightWidth;   // requested right-column width
    size_t headerRows;   // top band rows (0 => none); the rule sits on the last
    size_t footerRows;   // bottom band rows under the centre column (0 => none)
    const TerminalImage *background;   // borrowed, nullable
    TerminalRule rule;   // connector glyph set used by drawRules
} TerminalFrame;

TerminalFrame TerminalFrame_0(void);
TerminalFrame TerminalFrame_2(size_t leftWidth, size_t rightWidth);
#define TerminalFrame(...) CONSTRUCTOR_DISPATCH(TerminalFrame, __VA_ARGS__)
#define TerminalFrame_zero() TerminalFrame_0()

bool TerminalFrame_setRails(TerminalFrame *self, size_t leftWidth, size_t rightWidth);
TerminalFramePair TerminalFrame_getRails(const TerminalFrame *self);
bool TerminalFrame_setBands(TerminalFrame *self, size_t headerRows, size_t footerRows);
TerminalFramePair TerminalFrame_getBands(const TerminalFrame *self);
bool TerminalFrame_setBackground(TerminalFrame *self, const TerminalImage *image);
const TerminalImage *TerminalFrame_getBackground(const TerminalFrame *self);
// Rows/columns actually used, given a root size (rails may collapse to zero on a
// narrow terminal so the centre keeps at least one column).
TerminalFrameLayout TerminalFrame_getLayout(const TerminalFrame *self, const TerminalCanvas *root);

// Paint the borrowed background image across the whole root (no-op when none).
void TerminalFrame_paintBackground(const TerminalFrame *self, TerminalCanvas *root);

// --- Regions (all clipped; invalid when the root is too small) ---
TerminalCanvas TerminalFrame_header(const TerminalFrame *self, const TerminalCanvas *root);
TerminalCanvas TerminalFrame_left(const TerminalFrame *self, const TerminalCanvas *root);
TerminalCanvas TerminalFrame_center(const TerminalFrame *self, const TerminalCanvas *root);
TerminalCanvas TerminalFrame_right(const TerminalFrame *self, const TerminalCanvas *root);
TerminalCanvas TerminalFrame_footer(const TerminalFrame *self, const TerminalCanvas *root);

// Draw the frame's own rules: the header run (with rail junctions) and the two
// rail columns, using the frame's TerminalRule connector set.
void TerminalFrame_drawRules(const TerminalFrame *self, TerminalCanvas *root, TerminalPen rule);
bool TerminalFrame_setRule(TerminalFrame *self, TerminalRule rule);
TerminalRule TerminalFrame_getRule(const TerminalFrame *self);

TerminalProjection TerminalFrame_toString(const TerminalFrame *self);
TerminalProjection TerminalFrame_toStringStruct(const TerminalFrame *self);

#endif
